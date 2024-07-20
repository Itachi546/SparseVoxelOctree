cmake_minimum_required(VERSION 3.8)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# set(CMAKE_TOOLCHAIN_FILE "C:/Packages/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "C:/Packages/vcpkg/installed/x64-windows/share")
project(vox-engine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE PROJECT_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/external/imgui/*.cpp)
file(GLOB_RECURSE PROJECT_HEADERS ${CMAKE_SOURCE_DIR}/src/*.h
                          ${CMAKE_SOURCE_DIR}/external/*.h)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS})

add_subdirectory(shaders/)
set(ENKITS_BUILD_C_INTERFACE OFF)
set(ENKITS_BUILD_EXAMPLES OFF)
add_subdirectory(external/enkiTS)

find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

set(PROJECT_LIBRARIES 
    glad::glad
    glm::glm
    volk::volk
    glfw
    enkiTS
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE 
        src/
        external/
    )

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)
target_compile_definitions(${PROJECT_NAME} PRIVATE   
        GLM_FORCE_XYZW_ONLY
        GLM_ENABLE_EXPERIMENTAL
        _CRT_SECURE_NO_WARNINGS
        VK_USE_PLATFORM_WIN32_KHR
        VULKAN_ENABLED
        NOMINMAX
    ) 

if(MSVC)    
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif(MSVC)
