#include "pch.h"
#include "voxel-app.h"

#include "input.h"
#include "math-utils.h"
#include "gfx/debug.h"
#include "gfx/camera.h"
#include "gfx/imgui-service.h"
#include "gfx/gpu-timer.h"
#include "gfx/scene.h"
#include "gfx/async-loader.h"
#include "rendering/rendering-utils.h"

#include <stb_image.h>
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

void VoxelApp::LoadTextures(std::vector<std::string> &textures) {
    RD::TextureDescription desc = RD::TextureDescription::Initialize(1024, 1024);
    desc.usageFlags = RD::TEXTURE_USAGE_SAMPLED_BIT | RD::TEXTURE_USAGE_TRANSFER_DST_BIT;
    int width, height, comp;
    for (auto &texture : textures) {
        int res = stbi_info(texture.c_str(), &width, &height, &comp);
        if (res == 0) {
            LOGE("Failed to get texture info from the file" + texture);
            return;
        }
        desc.width = width;
        desc.height = height;
        desc.format = RD::FORMAT_R8G8B8A8_UNORM;
        desc.mipMaps = 1;

        std::string name = std::filesystem::path(texture).filename().string();
        TextureID textureId = device->CreateTexture(&desc, name.c_str());
        asyncLoader->RequestTextureLoad(texture, textureId);
    }
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    Debug::Initialize();

    // Initialize CommandBuffer/Pool
    device = RD::GetInstance();
    QueueID graphicsQueue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);
    commandPool = device->CreateCommandPool(graphicsQueue, "RenderCommandPool");
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

    RD::BoundUniform globalBinding{
        .bindingType = RD::BINDING_TYPE_UNIFORM_BUFFER,
        .binding = 0,
        .resourceID = globalUB,
        .offset = 0,
    };

    std::shared_ptr<Scene> scene = std::make_shared<Scene>();

    std::vector<MeshGroup> meshes;
    std::vector<std::string> textures;

    auto begin = std::chrono::high_resolution_clock::now();
    asyncLoader = std::make_shared<AsyncLoader>();

    asyncLoader->Initialize();
    asyncLoader->Start();
    if (scene->LoadMeshes({"C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/Sponza/Sponza.gltf"}, meshes, textures)) {
        // Run a texture loader task
        // TextureLoader loader;
        // std::thread(loader.load);
        LoadTextures(textures);
        scene->PrepareDrawData(meshes);
        // loader.wait();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    LOG("Total time taken: " + std::to_string(float(duration) / 1000.0f) + "s");
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
            OnRenderUI();
            device->PrepareSwapchain(commandBuffer, RD::TEXTURE_LAYOUT_PRESENT_SRC);
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
    RD::AttachmentInfo colorAttachmentInfos = {
        .loadOp = RD::LOAD_OP_CLEAR,
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
    FenceID renderEndFence = device->GetRenderEndFence();
    device->WaitForFence(&renderEndFence, 1, UINT64_MAX);
    asyncLoader->Shutdown();
    device->Destroy(commandPool);
    device->Destroy(globalUB);
    Debug::Shutdown();
    ImGuiService::Shutdown();
}