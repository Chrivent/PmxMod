target_sources(PmxMod PRIVATE
        Program.h Program.cpp
        MotionTimelineBuilder.h MotionTimelineBuilder.cpp
        ResourceDirectories.h ResourceDirectories.cpp
        ShaderEffectController.h ShaderEffectController.cpp
        Shader/ShaderPackage.h Shader/ShaderPackage.cpp
        Shader/InternalShaderCatalog.h Shader/InternalShaderCatalog.cpp
        Config.h Config.cpp
        Gui/GuiBackBuffer.h Gui/GuiBackBuffer.cpp
        Gui/GuiDrawer.h Gui/GuiDrawer.cpp
        Gui/FpsOverlay.h Gui/FpsOverlay.cpp
        Gui/GuiTheme.h Gui/GuiTheme.cpp
        Language.h Language.cpp
        MenuBar.h MenuBar.cpp
        PanelWindow.h PanelWindow.cpp
        RendererType.h
        Settings.h Settings.cpp
        Sound.h Sound.cpp
        TaskExecutor.h TaskExecutor.cpp

        Manager/CameraManager.h Manager/CameraManager.cpp
        Manager/InputManager.h Manager/InputManager.cpp
        Manager/PanelManager.h Manager/PanelManager.cpp

		Panel/Panel.h
        Panel/CameraPanel.h Panel/CameraPanel.cpp
        Panel/InformationPanel.h Panel/InformationPanel.cpp
        Panel/InterpolationCurvePanel.h Panel/InterpolationCurvePanel.cpp
        Panel/ModelPanel.h Panel/ModelPanel.cpp
        Panel/MotionPanel.h Panel/MotionPanel.cpp
        Panel/PlaybackPanel.h Panel/PlaybackPanel.cpp
        Panel/SoundPanel.h Panel/SoundPanel.cpp
)

target_compile_features(PmxMod PRIVATE cxx_std_23)

target_include_directories(PmxMod PRIVATE
        ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(PmxMod PRIVATE
        PmxMod::Core
        PmxMod::ProjectWarnings
        PmxMod::Viewer
        comctl32
        dwmapi
        nlohmann_json::nlohmann_json
		shell32
        uxtheme
)

set(RESOURCE_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/resource")
set(RESOURCE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/resource")

add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${RESOURCE_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RESOURCE_SOURCE_DIRECTORY}" "${RESOURCE_OUTPUT_DIRECTORY}"
        VERBATIM
)

add_dependencies(PmxMod SyncResources)
