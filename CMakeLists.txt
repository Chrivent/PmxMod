cmake_minimum_required(VERSION 4.1.2)

project(PmxMod LANGUAGES CXX)

set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Path to vcpkg root")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
set(VCPKG_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include")
list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
list(PREPEND CMAKE_MODULE_PATH "${VCPKG_INSTALLED_DIR}/share/stb")

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb MODULE REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h HINTS "${VCPKG_INCLUDE_DIR}" REQUIRED)
find_library(
        SHADERC_LIBRARY
        shaderc_shared
        HINTS "$ENV{VULKAN_SDK}/Lib" "${VCPKG_INSTALLED_DIR}/lib"
        REQUIRED
)
find_file(
        SHADERC_DLL
        shaderc_shared.dll
        HINTS "$ENV{VULKAN_SDK}/Bin" "${VCPKG_INSTALLED_DIR}/bin"
        REQUIRED
)

add_subdirectory(src/Core)
add_subdirectory(src/Viewer)

add_executable(PmxMod
        main.cpp

        src/Program/Program.h src/Program/Program.cpp
        src/Program/Config.h src/Program/Config.cpp
        src/Program/Gui/GuiBackBuffer.h src/Program/Gui/GuiBackBuffer.cpp
        src/Program/Gui/GuiDrawer.h src/Program/Gui/GuiDrawer.cpp
        src/Program/Gui/GuiTheme.h src/Program/Gui/GuiTheme.cpp
        src/Program/Language.h src/Program/Language.cpp
        src/Program/MenuBar.h src/Program/MenuBar.cpp
        src/Program/PanelWindow.h src/Program/PanelWindow.cpp
        src/Program/RendererType.h
        src/Program/Sound.h src/Program/Sound.cpp
        src/Program/TaskExecutor.h src/Program/TaskExecutor.cpp

        src/Program/Manager/CameraManager.h src/Program/Manager/CameraManager.cpp
        src/Program/Manager/InputManager.h src/Program/Manager/InputManager.cpp
        src/Program/Manager/PanelManager.h src/Program/Manager/PanelManager.cpp

        src/Program/Panel/Panel.h
        src/Program/Panel/CameraPanel.h
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
        "${VCPKG_INSTALLED_DIR}/bin/glfw3.dll"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)

add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADERC_DLL}"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)
