#include "voxel-app.h"
#include "input.h"
#include "math-utils.h"
#include "gfx/debug.h"
#include "gfx/camera.h"
#include "gfx/imgui-service.h"
#include "gfx/gpu-timer.h"
#include "voxels/parallel-octree.h"
#include "voxels/perlin-voxdata.h"
#include "voxels/voxel-data.h"
#include "voxels/octree-renderer.h"
#include "rendering/rendering-utils.h"

#include <glm/gtx/component_wise.hpp>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

static void GLAPIENTRY
MessageCallback(GLenum source,
                GLenum type,
                GLuint id,
                GLenum severity,
                GLsizei length,
                const GLchar *message,
                const void *userParam) {
    if (type != GL_DEBUG_TYPE_ERROR)
        return;
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
    assert(0);
}

void VoxelApp::LoadFromFile(const char *filename, float scale, uint32_t kOctreeDims) {
    VoxModelData *model = new VoxModelData();
    model->Load(filename, scale);
    {
        auto start = std::chrono::high_resolution_clock::now();
        octree->Generate(model);
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to generate chunk: " << duration << "ms" << std::endl;
    }
    model->Destroy();
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    Debug::Initialize();

    // Initialize CommandBuffer/Pool
    device = RD::GetInstance();
    commandPool = device->CreateCommandPool("RenderCommandPool");
    commandBuffer = device->CreateCommandBuffer(commandPool, "RenderCommandBuffer");

    Input::Singleton()->Initialize();
    ImGuiService::Initialize(glfwWindowPtr, commandBuffer);

    globalUB = device->CreateBuffer(sizeof(FrameData), RD::BUFFER_USAGE_UNIFORM_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "GlobalUniformBuffer");
    globalUBPtr = device->MapBuffer(globalUB);

    camera = new gfx::Camera();
    camera->SetPosition(glm::vec3{0.5f, 0.5f, 64.5f});
    camera->SetNearPlane(0.1f);

    dt = 0.0f;
    lastFrameTime = static_cast<float>(glfwGetTime());
    frameData.uLightPosition = glm::vec3(0.0f, 32.0f, 32.0f);
    origin = glm::vec3(32.0f);
    target = glm::vec3(0.0f);

#if 1 
    octree = new ParallelOctree("terrain.octree");
#else
    constexpr uint32_t kOctreeDims = 256;
    octree = new ParallelOctree(glm::vec3(0.0f), kOctreeDims);

    VoxelData* data = new PerlinVoxData();
    octree->Generate(data);
    // LoadFromFile("assets/models/dragon.vox", 1.0f, kOctreeDims);
    octree->Serialize("terrain.octree");
#endif
    octreeRenderer = new OctreeRenderer();
    octreeRenderer->Initialize(1920, 1080, octree);

    RD::BoundUniform globalBinding{
        .bindingType = RD::BINDING_TYPE_UNIFORM_BUFFER,
        .binding = 0,
        .resourceID = globalUB,
        .offset = 0,
    };
    gUniSetRasterizer = device->CreateUniformSet(octreeRenderer->rasterizer->pipeline, &globalBinding, 1, 0, "GlobalUniformSetRasterizer");
    gUniSetRaycast = device->CreateUniformSet(octreeRenderer->raycaster->pipeline, &globalBinding, 1, 0, "GlobalUniformSetRaycaster");
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();

        if (!AppWindow::minimized) {
            Debug::NewFrame();
            OnUpdate();
            device->BeginFrame();
            device->BeginCommandBuffer(commandBuffer);
            OnRender();
            device->CopyToSwapchain(commandBuffer, octreeRenderer->GetColorAttachment());
            OnRenderUI();
            device->PrepareSwapchain(commandBuffer);
            // Draw UI
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer);
            device->Present();
        } else
            std::this_thread::sleep_for(1ms);

        glfwSwapBuffers(glfwWindowPtr);

        float currentTime = static_cast<float>(glfwGetTime());
        dt = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        char buffer[64];
        sprintf_s(buffer, "frameTime: %.2fms", dt * 1000.0f);
        glfwSetWindowTitle(glfwWindowPtr, buffer);
    }
}

void VoxelApp::OnUpdate() {

    ImGuiService::NewFrame();
    if (!ImGuiService::IsFocused())
        UpdateControls();

    camera->Update(dt);

    if (camera->IsMoving()) {
        octreeRenderer->raycaster->spp = 0.0f;
    }

    frameData.uInvP = camera->GetInvProjectionMatrix();
    frameData.uInvV = camera->GetInvViewMatrix();
    frameData.P = camera->GetProjectionMatrix();
    frameData.V = camera->GetViewMatrix();
    frameData.VP = camera->GetProjectionMatrix() * camera->GetViewMatrix();
    frameData.uCameraPosition = camera->GetPosition();
    frameData.uScreenWidth = windowSize.x;
    frameData.uScreenHeight = windowSize.y;
    frameData.time = lastFrameTime;
    std::memcpy(globalUBPtr, &frameData, sizeof(FrameData));

    Debug::AddRect(octree->center - octree->size, octree->center + octree->size);

    Input *input = Input::Singleton();
    input->Update();
}

void VoxelApp::OnRenderUI() {
    glm::vec3 camPos = camera->GetPosition();
    ImGui::Text("Camera Position: %.2f %.2f %.2f", camPos.x, camPos.y, camPos.z);
    if (ImGui::DragFloat3("Light Position", &frameData.uLightPosition[0], 0.1f, -100.0f, 100.0f)) {
        octreeRenderer->raycaster->spp = 0;
    }
    octreeRenderer->AddUI();

    RD::AttachmentInfo colorAttachmentInfos = {
        .loadOp = RD::LOAD_OP_LOAD,
        .storeOp = RD::STORE_OP_STORE,
        .clearColor = {0.5f, 0.5f, 0.5f, 1.0f},
        .clearDepth = 0,
        .clearStencil = 0,
        .attachment = TextureID{~0u},
    };

    RD::RenderingInfo renderingInfo = {
        .width = (uint32_t)windowSize.x,
        .height = (uint32_t)windowSize.y,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfos,
        .pDepthStencilAttachment = nullptr,
    };

    device->BeginRenderPass(commandBuffer, &renderingInfo);
    device->SetViewport(commandBuffer, 0, 0, (uint32_t)windowSize.x, (uint32_t)windowSize.y);
    ImGuiService::Render(commandBuffer);
    device->EndRenderPass(commandBuffer);
}

void VoxelApp::OnRender() {
    UniformSetID uniformSet = octreeRenderer->renderMode == RenderMode_Rasterizer ? gUniSetRasterizer : gUniSetRaycast;
    octreeRenderer->Render(commandBuffer, uniformSet);
}

void VoxelApp::OnMouseMove(float x, float y) {
    Input::Singleton()->SetMousePos(glm::vec2{x, y});
}

void VoxelApp::OnMouseScroll(float x, float y) {}

void VoxelApp::OnMouseButton(int button, int action) {
    Input::Singleton()->SetKeyState(button, action != GLFW_RELEASE);
}

void VoxelApp::OnKey(int key, int action) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(glfwWindowPtr, true);
    Input::Singleton()->SetKeyState(key, action != GLFW_RELEASE);
}

void VoxelApp::OnResize(float width, float height) {
    minimized = width == 0 || height == 0;
    bool resized = width != windowSize.x || height != windowSize.y;
    if (!minimized && resized) {
        windowSize.x = width;
        windowSize.y = height;
    }
}

void VoxelApp::UpdateControls() {
    const float rotateSpeed = 2.0f;
    Input *input = Input::Singleton();
    if (input->GetKey(GLFW_MOUSE_BUTTON_LEFT)->isDown) {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        camera->Rotate(-mouseDelta.y * rotateSpeed, -mouseDelta.x * rotateSpeed, dt);
    }

    float walkSpeed = dt * 4.0f;
    if (input->GetKey(GLFW_KEY_LEFT_SHIFT)->isDown)
        walkSpeed *= 4.0f;

    if (input->GetKey(GLFW_KEY_W)->isDown)
        camera->Walk(-walkSpeed);
    else if (input->GetKey(GLFW_KEY_S)->isDown)
        camera->Walk(walkSpeed);

    if (input->GetKey(GLFW_KEY_A)->isDown)
        camera->Strafe(-walkSpeed);
    else if (input->GetKey(GLFW_KEY_D)->isDown)
        camera->Strafe(walkSpeed);

    if (input->GetKey(GLFW_KEY_1)->isDown)
        camera->Lift(walkSpeed);
    else if (input->GetKey(GLFW_KEY_2)->isDown)
        camera->Lift(-walkSpeed);
}

VoxelApp::~VoxelApp() {
    device->Destroy(commandPool);
    device->Destroy(globalUB);
    device->Destroy(gUniSetRasterizer);
    device->Destroy(gUniSetRaycast);
    Debug::Shutdown();
    ImGuiService::Shutdown();
    // GpuTimer::Shutdown();
    if (octree)
        delete octree;

    octreeRenderer->Shutdown();
    delete octreeRenderer;
}