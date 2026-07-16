cmake_minimum_required(VERSION 4.1.2)

project(PmxMod LANGUAGES CXX)

add_compile_definitions(NOMINMAX)

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

add_executable(PmxMod
        main.cpp

        src/Program/Program.h src/Program/Program.cpp
		src/Program/ResourceDirectories.h src/Program/ResourceDirectories.cpp
        src/Program/Config.h src/Program/Config.cpp
        src/Program/Gui/GuiBackBuffer.h src/Program/Gui/GuiBackBuffer.cpp
        src/Program/Gui/GuiDrawer.h src/Program/Gui/GuiDrawer.cpp
        src/Program/Gui/FpsOverlay.h src/Program/Gui/FpsOverlay.cpp
        src/Program/Gui/GuiTheme.h src/Program/Gui/GuiTheme.cpp
        src/Program/Language.h src/Program/Language.cpp
        src/Program/MenuBar.h src/Program/MenuBar.cpp
        src/Program/PanelWindow.h src/Program/PanelWindow.cpp
        src/Program/RendererType.h
        src/Program/Settings.h src/Program/Settings.cpp
        src/Program/Sound.h src/Program/Sound.cpp
        src/Program/TaskExecutor.h src/Program/TaskExecutor.cpp

        src/Program/Manager/CameraManager.h src/Program/Manager/CameraManager.cpp
        src/Program/Manager/InputManager.h src/Program/Manager/InputManager.cpp
        src/Program/Manager/PanelManager.h src/Program/Manager/PanelManager.cpp

        src/Program/Panel/Panel.h src/Program/Panel/Panel.cpp
        src/Program/Panel/CameraPanel.h src/Program/Panel/CameraPanel.cpp
        src/Program/Panel/InterpolationCurvePanel.h src/Program/Panel/InterpolationCurvePanel.cpp
        src/Program/Panel/ModelPanel.h src/Program/Panel/ModelPanel.cpp
        src/Program/Panel/MotionPanel.h src/Program/Panel/MotionPanel.cpp
        src/Program/Panel/PlaybackPanel.h src/Program/Panel/PlaybackPanel.cpp
        src/Program/Panel/SoundPanel.h src/Program/Panel/SoundPanel.cpp
)

target_compile_features(PmxMod PRIVATE cxx_std_23)

target_include_directories(PmxMod PRIVATE
        ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(PmxMod PRIVATE
        PmxMod::Core
        PmxMod::Viewer
        comctl32
        dwmapi
        nlohmann_json::nlohmann_json
        uxtheme
)

set(RESOURCE_SRC_DIR "${CMAKE_SOURCE_DIR}/resource")
set(RESOURCE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")

add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${RESOURCE_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RESOURCE_SRC_DIR}" "${RESOURCE_OUT_DIR}"
        VERBATIM
)

add_dependencies(PmxMod SyncResources)

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
