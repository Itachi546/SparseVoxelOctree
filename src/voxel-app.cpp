#include "pch.h"
#include "voxel-app.h"

#include "input.h"
#include "math-utils.h"
#include "gfx/debug.h"
#include "gfx/camera.h"
#include "ui/imgui-service.h"
#include "gfx/gpu-timer.h"
#include "gfx/gltf-scene.h"
#include "gfx/async-loader.h"
#include "rendering/rendering-utils.h"

// @TODO Temp
#include "sparse-octree/voxelizer.h"

#include <glm/gtx/component_wise.hpp>

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

TextureID VoxelApp::CreateSwapchainDepthAttachment() {
    RD::TextureDescription desc = RD::TextureDescription::Initialize((uint32_t)windowSize.x, (uint32_t)windowSize.y);
    desc.usageFlags = RD::TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT | RD::TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT;
    desc.format = RD::FORMAT_D24_UNORM_S8_UINT;
    return device->CreateTexture(&desc, "Swapchain Depth Attachment");
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    Debug::Initialize();

    // Initialize CommandBuffer/Pool
    device = RD::GetInstance();
    QueueID graphicsQueue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);
    commandPool = device->CreateCommandPool(graphicsQueue, "RenderCommandPool");
    commandBuffer = device->CreateCommandBuffer(commandPool, "RenderCommandBuffer");
    renderFence = device->CreateFence("Render Fence");

    Input::Singleton()->Initialize();
    ImGuiService::Initialize(glfwWindowPtr, commandBuffer);

    globalUB = device->CreateBuffer(sizeof(FrameData), RD::BUFFER_USAGE_UNIFORM_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "GlobalUniformBuffer");
    globalUBPtr = device->MapBuffer(globalUB);

    depthAttachment = CreateSwapchainDepthAttachment();

    camera = new gfx::Camera();
    camera->SetPosition(glm::vec3{0.5f, 2.0f, 0.5f});
    camera->SetRotation(glm::vec3(0.0f, glm::radians(90.0f), 0.0f));
    camera->SetNearPlane(0.1f);

    dt = 0.0f;
    lastFrameTime = static_cast<float>(glfwGetTime());
    frameData.uLightPosition = glm::vec3(0.0f, 32.0f, 32.0f);
    origin = glm::vec3(32.0f);
    target = glm::vec3(0.0f);

    auto begin = std::chrono::high_resolution_clock::now();

    asyncLoader = std::make_shared<AsyncLoader>();
    scene = std::make_shared<GLTFScene>();

    asyncLoader->Initialize(scene);
    asyncLoader->Start();

    // const std::string meshPath = "C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/NewSponza/NewSponza_Main_glTF_002.gltf";
    const std::string meshPath = "C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/Sponza/Sponza.gltf";
    if (scene->Initialize({/*meshPath /*,*/ "C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/dragon/dragon.glb"}, asyncLoader)) {
        scene->PrepareDraws(globalUB);
    } else
        LOGE("Failed to initialize scene");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    LOG("Total time taken: " + std::to_string(float(duration) / 1000.0f) + "s");

    voxelizer = std::make_shared<Voxelizer>();

    voxelizer->Initialize(scene);
    voxelizer->Voxelize(commandPool, commandBuffer);
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();

        if (!AppWindow::minimized) {
            Debug::NewFrame();
            OnUpdate();
            device->BeginFrame();
            device->BeginCommandBuffer(commandBuffer);
            device->PrepareSwapchain(commandBuffer, RD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            OnRender();
            device->PrepareSwapchain(commandBuffer, RD::TEXTURE_LAYOUT_PRESENT_SRC);

            std::static_pointer_cast<GLTFScene>(scene)->UpdateTextures(commandBuffer);
            // Draw UI
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer, renderFence);
            device->Present();

            device->WaitForFence(&renderFence, 1, UINT64_MAX);
            device->ResetFences(&renderFence, 1);
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

    Input *input = Input::Singleton();
    input->Update();
}

void VoxelApp::OnRenderUI() {
    glm::vec3 camPos = camera->GetPosition();
    ImGui::Text("Camera Position: %.2f %.2f %.2f", camPos.x, camPos.y, camPos.z);

    float memoryUsage = (float)device->GetMemoryUsage() / (1024.0f * 1024.0f);
    ImGui::Text("GPU Memory Usage: %.2fMB", memoryUsage);
    ImGuiService::Render(commandBuffer);
}

void VoxelApp::OnRender() {

    RD::TextureBarrier barrier{
        .texture = depthAttachment,
        .srcAccess = 0,
        .dstAccess = RD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .newLayout = RD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamily = QUEUE_FAMILY_IGNORED,
        .dstQueueFamily = QUEUE_FAMILY_IGNORED,
        .baseMipLevel = 0,
        .baseArrayLayer = 0,
        .levelCount = UINT32_MAX,
        .layerCount = UINT32_MAX,
    };
    device->PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, &barrier, 1);

    RD::AttachmentInfo colorAttachmentInfos = {
        .loadOp = RD::LOAD_OP_CLEAR,
        .storeOp = RD::STORE_OP_STORE,
        .clearColor = {0.5f, 0.5f, 0.5f, 1.0f},
        .clearDepth = 0,
        .clearStencil = 0,
        .attachment = TextureID{INVALID_TEXTURE_ID},
    };

    RD::AttachmentInfo depthStencilAttachmentInfo = {
        .loadOp = RD::LOAD_OP_CLEAR,
        .storeOp = RD::STORE_OP_STORE,
        .clearDepth = 1.0f,
        .clearStencil = 0,
        .attachment = depthAttachment,
    };

    RD::RenderingInfo renderingInfo = {
        .width = (uint32_t)windowSize.x,
        .height = (uint32_t)windowSize.y,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfos,
        .pDepthStencilAttachment = &depthStencilAttachmentInfo,
    };

    device->BeginRenderPass(commandBuffer, &renderingInfo);

    device->SetViewport(commandBuffer, 0.0f, windowSize.y, windowSize.x, -windowSize.y);
    device->SetScissor(commandBuffer, 0, 0, (uint32_t)windowSize.x, (uint32_t)windowSize.y);

    scene->Render(commandBuffer);

    OnRenderUI();

    device->EndRenderPass(commandBuffer);
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

        device->Destroy(depthAttachment);
        depthAttachment = CreateSwapchainDepthAttachment();
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
    asyncLoader->Shutdown();
    voxelizer->Shutdown();
    scene->Shutdown();
    device->Destroy(depthAttachment);
    device->Destroy(commandPool);
    device->Destroy(globalUB);
    device->Destroy(renderFence);
    Debug::Shutdown();
    ImGuiService::Shutdown();
}