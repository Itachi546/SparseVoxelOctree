#include "voxel-app.h"

#include "math-utils.h"

#include "gfx/mesh.h"
#include "gfx/camera.h"
#include "gfx/debug.h"
#include "voxels/octree.h"
#include "voxels/density-generator.h"
#include "voxels/voxel-data.h"
#include "voxels/octree-raycaster.h"

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

std::vector<uint32_t> brick;
bool RaycastDDA(std::vector<AABB> &aabbs) {
    // const glm::vec3 r0 = glm::vec3(18.5f, 10.0f, 5.0f);
    const glm::vec3 r0 = glm::vec3(0.0f, 3.0f, 2.5f);
    const glm::vec3 rd = normalize(glm::vec3(1.0f, 1.0f, 0.5f));
    Debug::AddLine(r0, r0 + rd * 30.0f, glm::vec3(1.0f));

    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(16.0f);
    Debug::AddRect(min, max);

    glm::vec3 stepDir = glm::sign(rd);
    glm::vec3 tStep = 1.0f / (glm::abs(rd) + 0.0001f);

    glm::vec3 fp = r0;
    glm::vec3 p = glm::floor(r0);
    glm::vec3 t = (fp - p) * tStep;
    const int iteration = 20;
    for (int i = 0; i < iteration; ++i) {
        glm::vec3 nearestAxis = glm::step(t, glm::vec3(t.y, t.z, t.x)) * glm::step(t, glm::vec3(t.z, t.x, t.y));
        p += nearestAxis * stepDir;
        t += nearestAxis * tStep;

        aabbs.push_back(AABB{p, p + 1.0f});
        if (p.x >= 0.0f && p.x < 16.0f && p.y >= 0.0f && p.y < 16.0f && p.z >= 0.0f && p.z < 16.0f) {
            uint32_t voxelIndex = int(p.x) * BRICK_SIZE * BRICK_SIZE + int(p.y) * BRICK_SIZE + int(p.z);
            if (brick[voxelIndex] > 0)
                return true;
        }
    }
    return false;
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    fullscreenShader.Create("assets/shaders/instanced.vert",
                            "assets/shaders/instanced.frag");

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
    octree = new Octree("suzanne.octree");
#else
    voxelCount = 0;
    const uint32_t kOctreeDims = 32;
    octree = new Octree(glm::vec3(0.0f), float(kOctreeDims));
    VoxModelData model;
    model.Load("assets/models/suzanne.vox", 0.4f);
    {
        auto start = std::chrono::high_resolution_clock::now();
        octree->Generate(&model);
        // octree->Serialize("pieta.octree");
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
#endif
    std::vector<glm::vec4> voxels;
    /*
     for (int x = 0; x < 16; ++x) {
         for (int y = 0; y < 16; ++y) {
             for (int z = 0; z < 16; ++z) {
                 glm::vec3 p = glm::vec3(x, y, z);
                 float len = glm::length(p - 8.0f) - 6.0f;
                 if (len < 0.0f) {
                     brick.push_back(0xff);
                     voxels.push_back(glm::vec4(p + 0.5f, 0.5f));
                 } else
                     brick.push_back(0);
             }
         }
     }
     */
    octree->ListVoxels(voxels);
    voxelCount = static_cast<uint32_t>(voxels.size());
    if (voxels.size() > 0)
        glNamedBufferSubData(instanceBuffer, 0, sizeof(glm::vec4) * voxels.size(), voxels.data());
    raycaster = new OctreeRaycaster();
    raycaster->Initialize(octree);
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

std::vector<AABB> aabbs;
int debugIndex = 1;
struct Key {
    bool wasDown;
    bool isDown;
} keyUp, keyDown, keyHide;
bool showMesh = true;

void VoxelApp::OnUpdate() {
    UpdateControls();
    camera->Update(dt);

    const glm::vec3 r0 = glm::vec3(35.5f, 30.5f, 30.5f);
    const glm::vec3 rd = normalize(-r0);
    Ray ray{r0, rd};
    Debug::AddRect(octree->center - octree->size, octree->center + octree->size);
    glm::vec3 intersection, normal;

    if (octree->Raycast(ray.origin, ray.direction, intersection, normal, aabbs)) {
        AABB &aabb = aabbs.back();
        Debug::AddRect(aabb.min, aabb.max, glm::vec3{1.0f});
    }

    keyUp.wasDown = keyUp.isDown;
    keyUp.isDown = glfwGetKey(AppWindow::glfwWindowPtr, GLFW_KEY_E) == GLFW_PRESS;
    if (!keyUp.wasDown && keyUp.isDown)
        debugIndex++;

    keyDown.wasDown = keyDown.isDown;
    keyDown.isDown = glfwGetKey(AppWindow::glfwWindowPtr, GLFW_KEY_R) == GLFW_PRESS;
    if (!keyDown.wasDown && keyDown.isDown)
        debugIndex--;

    keyHide.wasDown = keyHide.isDown;
    keyHide.isDown = glfwGetKey(AppWindow::glfwWindowPtr, GLFW_KEY_H) == GLFW_PRESS;
    if (!keyHide.wasDown && keyHide.isDown)
        showMesh = !showMesh;

    debugIndex = std::min(std::max(debugIndex, 0), (int)aabbs.size() - 1);
    for (int i = 0; i < debugIndex; ++i) {
        Debug::AddRect(aabbs[i].min, aabbs[i].max, COLORS[i % 4]);
    }

    // RaycastDDA(aabbs);
    // for (int i = 0; i < aabbs.size(); ++i)
    // Debug::AddRect(aabbs[i].min, aabbs[i].max);
    // aabbs.clear();

    // if (loadList.size() > 0)
    // ProcessLoadList();
    Debug::AddLine(ray.origin, ray.GetPointAt(200.0f), glm::vec3(1.0f));
}

void VoxelApp::OnRender() {
    glm::mat4 VP = camera->GetViewProjectionMatrix();
#if 1
    if (voxelCount > 0 && showMesh) {
        fullscreenShader.Bind();
        fullscreenShader.SetUniformMat4("uVP", &VP[0][0]);
        fullscreenShader.SetBuffer(0, instanceBuffer);
        cubeMesh->DrawInstanced(voxelCount);
        fullscreenShader.Unbind();
    }
#else
    raycaster->Render(camera);
#endif
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
    //  delete octree;

    fullscreenShader.Destroy();
    gfx::DestroyBuffer(instanceBuffer);
    delete cubeMesh;
}