cmake_minimum_required(VERSION 4.1.2)

project(PmxMod LANGUAGES CXX)

option(PMXMOD_BUILD_TESTS "PmxMod 테스트를 빌드합니다" OFF)

add_compile_definitions(NOMINMAX)

add_library(PmxModProjectWarnings INTERFACE)
add_library(PmxMod::ProjectWarnings ALIAS PmxModProjectWarnings)

target_compile_options(PmxModProjectWarnings INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb MODULE REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_library(DXCOMPILER_LIBRARY dxcompiler HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
find_file(DXCOMPILER_DLL dxcompiler.dll HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)

add_subdirectory(src/Core)
add_subdirectory(src/Viewer)

add_executable(PmxMod main.cpp)
add_subdirectory(src/Program)

add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:glfw>"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)

add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL}"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)

if(PMXMOD_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
