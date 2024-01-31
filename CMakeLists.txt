cmake_minimum_required(VERSION 3.8)

# set(CMAKE_TOOLCHAIN_FILE "C:/Packages/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "C:/Packages/vcpkg/installed/x64-windows/share")
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

project(vox-engine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

SET(PROJECT_SOURCE_FILES 
                  "src/gfx/opengl.cpp"
                  "src/main.cpp"
                  "src/gfx/mesh.cpp"
                  "src/gfx/camera.cpp"
                  "src/voxels/octree.cpp"
                  "src/utils.cpp"
                  "src/voxel-app.cpp")
SET (PROJECT_HEADER_FILES 
                   "src/gfx/opengl.h"
                   "src/app-window.h"
                   "src/utils.h"
                   "src/gfx/camera.h"
                   "src/math-utils.h"
                   "src/voxels/octree.h"
                   "src/gfx/mesh.h"
                   "src/voxel-app.h")

add_executable(${PROJECT_NAME} ${PROJECT_HEADER_FILES} ${PROJECT_SOURCE_FILES})

find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

set(PROJECT_LIBRARIES 
    glad::glad
    glm::glm
    glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE src/)
target_compile_definitions(${PROJECT_NAME} PRIVATE   
        GLM_FORCE_XYZW_ONLY
    ) 