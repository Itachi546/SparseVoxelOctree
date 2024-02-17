cmake_minimum_required(VERSION 3.8)

# set(CMAKE_TOOLCHAIN_FILE "C:/Packages/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "C:/Packages/vcpkg/installed/x64-windows/share")
project(vox-engine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

SET(PROJECT_SOURCE_FILES 
                  "src/gfx/opengl.cpp"
                  "src/main.cpp"
                  "src/input.cpp"
                  "src/gfx/mesh.cpp"
                  "src/gfx/camera.cpp"
                  "src/gfx/debug.cpp"
                  "src/voxels/octree.cpp"
                  "src/voxels/voxel-data.cpp"
                  "src/voxels/octree-raycaster.cpp"
                  "src/utils.cpp"
                  "src/voxel-app.cpp")
SET (PROJECT_HEADER_FILES 
                   "src/gfx/opengl.h"
                   "src/app-window.h"
                   "src/utils.h"
                   "src/input.h"
                   "src/gfx/camera.h"
                   "src/math-utils.h"
                   "src/gfx/debug.h"
                   "src/voxels/octree.h"
                   "src/voxels/octree-raycaster.h"
                   "src/voxels/density-generator.h"
                   "src/voxels/voxel-data.h"
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
target_include_directories(${PROJECT_NAME} PRIVATE 
        src/
        external/)
target_compile_definitions(${PROJECT_NAME} PRIVATE   
        GLM_FORCE_XYZW_ONLY
        _CRT_SECURE_NO_WARNINGS
    ) 

if(MSVC)    
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif(MSVC)
