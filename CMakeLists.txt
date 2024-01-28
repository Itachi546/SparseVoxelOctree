cmake_minimum_required(VERSION 3.20.0)

# set(CMAKE_TOOLCHAIN_FILE "C:/Packages/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "C:/Packages/vcpkg/installed/x64-windows/share")

project(vox-engine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

SET(PRJ_SRC_FILES src/main.cpp)
SET (PRJ_HEADER_FILES)

add_executable(${PROJECT_NAME} ${PRJ_HEADER_FILES} ${PRJ_SRC_FILES})

find_package(volk CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

set(PRJ_LIBRARIES 
 volk::volk
 glm::glm
 glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PRJ_LIBRARIES})

