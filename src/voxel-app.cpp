#include "voxel-app.h"
#include "input.h"
#include "math-utils.h"

#include "gfx/camera.h"
#include "gfx/debug.h"
#include "gfx/imgui-service.h"
#include "gfx/gpu-timer.h"
#include "voxels/octree.h"
#include "voxels/voxel-data.h"
#include "voxels/octree-raycaster.h"
#include "voxels/octree-rasterizer.h"
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
    octree = new Octree(glm::vec3(0.0f), float(kOctreeDims));
    VoxModelData model;
    model.Load(filename, scale);
    {
        auto start = std::chrono::high_resolution_clock::now();
        octree->Generate(&model);
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to generate chunk: " << duration << "ms" << std::endl;
    }
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to read/load chunk: " << duration << "ms" << std::endl;
    }
    model.Destroy();
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    // Debug::Initialize();
    // GpuTimer::Initialize();
    Input::Singleton()->Initialize();
    // ImGuiService::Initialize(glfwWindowPtr);

    // Initialize CommandBuffer/Pool
    device = RD::GetInstance();
    commandPool = device->CreateCommandPool("RenderCommandPool");
    commandBuffer = device->CreateCommandBuffer(commandPool, "RenderCommandBuffer");

    globalUB = device->CreateBuffer(sizeof(FrameData), RD::BUFFER_USAGE_UNIFORM_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "GlobalUniformBuffer");
    globalUBPtr = device->MapBuffer(globalUB);

    camera = new gfx::Camera();
    camera->SetPosition(glm::vec3{0.0f, 0.0f, 32.0f});

    dt = 0.0f;
    lastFrameTime = static_cast<float>(glfwGetTime());
    frameData.uLightPosition = glm::vec3(0.0f, 32.0f, 32.0f);
    origin = glm::vec3(32.0f);
    target = glm::vec3(0.0f);

#if 1
    octree = new Octree("monu3x16.octree");
#else
    constexpr uint32_t kOctreeDims = 32;
    LoadFromFile("assets/models/monu3.vox", 0.5f, kOctreeDims);
    octree->Serialize("monu3x16.octree");
#endif
    raycaster = new OctreeRaycaster();
    raycaster->Initialize(octree, 1920, 1080);

    // rasterizer = new OctreeRasterizer();
    // rasterizer->Initialize(octree);

    RD::BoundUniform globalBinding{
        .bindingType = RD::BINDING_TYPE_UNIFORM_BUFFER,
        .binding = 0,
        .resourceID = globalUB,
        .offset = 0,
    };
    globalUniformSet = device->CreateUniformSet(raycaster->pipeline, &globalBinding, 1, 0, "GlobalUniformSet");
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();

        if (!AppWindow::minimized) {
            OnUpdate();
            device->BeginFrame();
            device->BeginCommandBuffer(commandBuffer);
            OnRender();
            // OnRenderUI();
            device->CopyToSwapchain(commandBuffer, raycaster->outputTexture);
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
    UpdateControls();

    camera->Update(dt);

    frameData.uInvP = camera->GetInvProjectionMatrix();
    frameData.uInvV = camera->GetInvViewMatrix();
    frameData.uCameraPosition = camera->GetPosition();
    frameData.uScreenWidth = 1920;
    frameData.uScreenHeight = 1080;
    std::memcpy(globalUBPtr, &frameData, sizeof(FrameData));

    /*
    ImGuiService::NewFrame();

    if (!ImGuiService::IsFocused())
        UpdateControls();

    */

    /*
#if 1
    Ray ray;
    // glm::vec2 mouseCoord = ConvertFromWindowToNDC(input->mousePos, windowSize);
    // camera->GenerateCameraRay(&ray, mouseCoord);
    ray.origin = origin;
    ray.direction = glm::normalize(target - origin);
    // Debug::AddLine(ray.origin, ray.GetPointAt(500.0f));

    RayHit hit = octree->Raycast(ray.origin, ray.direction);
    if (hit.intersect) {
        ImGui::Text("Intersect");
        ImGui::Text("Hit distance: %.2f", hit.t);
        glm::vec3 p = ray.GetPointAt(hit.t);
        // Debug::AddRect(p - 0.005f, p + 0.005f, glm::vec3(1.0f));
    }
#endif
    */
    Input *input = Input::Singleton();
    input->Update();
}

void VoxelApp::OnRenderUI() {
    ImGui::Text("Total Voxel: %d", rasterizer->voxelCount);
    ImGui::Checkbox("Show", &show);

    if (show)
        ImGui::Checkbox("Rasterizer", &enableRasterizer);

    ImGui::SliderFloat3("Ray origin", &origin[0], -64.0f, 64.0f);
    ImGui::SliderFloat3("Ray target", &target[0], -64.0f, 64.0f);

    ImGui::DragFloat3("Light Position", &lightPosition[0], 0.1f, -100.0f, 100.0f);
    Debug::AddRect(lightPosition - 0.5f, lightPosition + 0.5f, glm::vec3(1.0f));

    GpuTimer::AddUI();

    ImGuiService::Render();
    GpuTimer::Reset();
}

void VoxelApp::OnRender() {
    glm::mat4 VP = camera->GetViewProjectionMatrix();
    if (show) {
        if (enableRasterizer) {
            rasterizer->Render(camera);
        } else
            raycaster->Render(commandBuffer, globalUniformSet);
    }
    // Debug::Render(VP);
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

    // ImGuiService::Shutdown();
    // GpuTimer::Shutdown();
    if (octree)
        delete octree;
    delete raycaster;
    // delete rasterizer;
}