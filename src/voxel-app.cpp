#include "voxel-app.h"

#include "math-utils.h"

#include "gfx/mesh.h"
#include "gfx/camera.h"
#include "gfx/debug.h"
#include "voxels/octree.h"
#include "voxels/density-generator.h"

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

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    fullscreenShader.Create("../../assets/shaders/instanced.vert",
                            "../../assets/shaders/instanced.frag");

    Debug::Initialize();

    cubeMesh = new gfx::Mesh();
    gfx::Mesh::CreateCube(cubeMesh);

    uint32_t maxEntity = 10'000'000;
    instanceBuffer = gfx::CreateBuffer(nullptr, maxEntity * sizeof(glm::vec4), GL_DYNAMIC_STORAGE_BIT);

    camera = new gfx::Camera();
    camera->SetPosition(glm::vec3{0.0f, 0.0f, 32.0f});

    mouseDown = false;
    mouseDelta = glm::vec2(0.0f, 0.0f);
    mousePos = glm::vec2(0.0f, 0.0f);

    dt = 0.0f;
    lastFrameTime = static_cast<float>(glfwGetTime());

#if 0
    octree = new Octree("../../scenex4.octree");
    std::vector<glm::vec4> voxels;
    octree->GenerateVoxels(voxels);
    voxelCount = static_cast<uint32_t>(voxels.size());
    if (voxels.size() > 0)
        glNamedBufferSubData(instanceBuffer, 0, sizeof(glm::vec4) * voxels.size(), voxels.data());
#else
    voxelCount = 0;
    const uint32_t kOctreeDims = 32;
    float halfSize = LEAF_NODE_SCALE * 0.5f;

    octree = new Octree(glm::vec3(0.0f), float(kOctreeDims));
    /*
    tempBrick = new OctreeBrick();
    int DIMS = kOctreeDims / LEAF_NODE_SCALE;
    for (int x = -DIMS; x < DIMS; ++x)
        for (int y = -DIMS; y < DIMS; ++y)
            for (int z = -DIMS; z < DIMS; ++z)
                loadList.push_back(glm::vec3{x, y, z} * float(LEAF_NODE_SCALE) + halfSize);

    glm::vec3 camPos = camera->GetPosition();
    std::sort(loadList.begin(), loadList.end(),
              [&camPos](const glm::vec3 &p0, const glm::vec3 &p1) {
                  return length(p0 - camPos) > length(p1 - camPos);
              });
              */
    generator = new DensityGenerator();

    {
        auto start = std::chrono::high_resolution_clock::now();
        octree->Generate(generator);
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to generate chunk: " << duration << "ms" << std::endl;
    }
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<glm::vec4> voxels;
        octree->ListVoxels(voxels);
        voxelCount = static_cast<uint32_t>(voxels.size());
        if (voxels.size() > 0)
            glNamedBufferSubData(instanceBuffer, 0, sizeof(glm::vec4) * voxels.size(), voxels.data());
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to read/load chunk: " << duration << "ms" << std::endl;
    }
#endif
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, static_cast<int>(windowSize.x), static_cast<int>(windowSize.y));

        if (!AppWindow::minimized) {
            OnUpdate();
            OnRender();
        } else
            std::this_thread::sleep_for(1ms);

        glfwSwapBuffers(glfwWindowPtr);

        float currentTime = static_cast<float>(glfwGetTime());
        dt = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        char buffer[64];
        sprintf_s(buffer, "frameTime: %.2fms", dt * 1000.0f);
        glfwSetWindowTitle(glfwWindowPtr, buffer);

        mouseDelta = {0.0f, 0.0f};
    }
}

void VoxelApp::ProcessLoadList() {
    const uint32_t kNumLoadWork = 8;
    for (uint32_t workCount = 0; workCount < kNumLoadWork && loadList.size() > 0; ++workCount) {
        glm::vec3 position = loadList.back();
        loadList.pop_back();
        tempBrick->position = position;
        glm::vec3 min = position - LEAF_NODE_SCALE * 0.5f;
        float size = float(LEAF_NODE_SCALE);
        bool empty = true;
        for (int x = 0; x < BRICK_SIZE; ++x) {
            for (int y = 0; y < BRICK_SIZE; ++y) {
                for (int z = 0; z < BRICK_SIZE; ++z) {
                    glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / 16.0f;
                    glm::vec3 p = min + t01 * size;
                    float val = generator->Sample(p);
                    uint32_t index = x * BRICK_SIZE * BRICK_SIZE + y * BRICK_SIZE + z;
                    if (val <= 0.0f) {
                        tempBrick->data[index] = 0xff0000;
                        empty = false;
                    } else
                        tempBrick->data[index] = 0;
                }
            }
        }
        if (!empty) {
            octree->Insert(tempBrick);
        }
    }
    std::vector<glm::vec4> voxels;
    octree->ListVoxels(voxels);
    voxelCount = static_cast<uint32_t>(voxels.size());
    if (voxels.size() > 0)
        glNamedBufferSubData(instanceBuffer, 0, sizeof(glm::vec4) * voxels.size(), voxels.data());
}

void VoxelApp::OnUpdate() {
    UpdateControls();
    camera->Update(dt);

    if (loadList.size() > 0)
        ProcessLoadList();

    Debug::AddRect(octree->center - octree->size, octree->center + octree->size);
}

void VoxelApp::OnRender() {
    glm::mat4 VP = camera->GetViewProjectionMatrix();
    if (voxelCount > 0) {
        fullscreenShader.Bind();
        fullscreenShader.SetUniformMat4("uVP", &VP[0][0]);
        fullscreenShader.SetBuffer(0, instanceBuffer);
        cubeMesh->DrawInstanced(voxelCount);
        fullscreenShader.Unbind();
    }
    Debug::Render(VP);
}

void VoxelApp::OnMouseMove(float x, float y) {
    mouseDelta = {x - mousePos.x, y - mousePos.y};
    mousePos = {x, y};
}

void VoxelApp::OnMouseScroll(float x, float y) {}

void VoxelApp::OnMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_RELEASE)
            mouseDown = false;
        else
            mouseDown = true;
    }
}

void VoxelApp::OnKey(int key, int action) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(glfwWindowPtr, true);
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
    if (mouseDown)
        camera->Rotate(-mouseDelta.y * rotateSpeed, -mouseDelta.x * rotateSpeed, dt);

    float walkSpeed = dt * 4.0f;
    if (glfwGetKey(glfwWindowPtr, GLFW_KEY_LEFT_SHIFT) != GLFW_RELEASE)
        walkSpeed *= 4.0f;

    if (glfwGetKey(glfwWindowPtr, GLFW_KEY_W) != GLFW_RELEASE)
        camera->Walk(-walkSpeed);
    else if (glfwGetKey(glfwWindowPtr, GLFW_KEY_S) != GLFW_RELEASE)
        camera->Walk(walkSpeed);

    if (glfwGetKey(glfwWindowPtr, GLFW_KEY_A) != GLFW_RELEASE)
        camera->Strafe(-walkSpeed);
    else if (glfwGetKey(glfwWindowPtr, GLFW_KEY_D) != GLFW_RELEASE)
        camera->Strafe(walkSpeed);

    if (glfwGetKey(glfwWindowPtr, GLFW_KEY_1) != GLFW_RELEASE)
        camera->Lift(walkSpeed);
    else if (glfwGetKey(glfwWindowPtr, GLFW_KEY_2) != GLFW_RELEASE)
        camera->Lift(-walkSpeed);
}

VoxelApp::~VoxelApp() {
    if (loadList.size() == 0) {
        octree->Serialize("../../scenex4.octree");
    }
    delete octree;

    fullscreenShader.Destroy();
    gfx::DestroyBuffer(instanceBuffer);
    delete cubeMesh;
}