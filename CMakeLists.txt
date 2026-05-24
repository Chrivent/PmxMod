cmake_minimum_required(VERSION 3.20)

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
find_package(Vulkan REQUIRED)
find_package(Stb MODULE REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h HINTS "${VCPKG_INCLUDE_DIR}" REQUIRED)
find_program(GLSLC_EXECUTABLE
        NAMES glslc
        HINTS "$ENV{VULKAN_SDK}/Bin"
        REQUIRED
)

add_executable(PmxMod
        main.cpp

        src/Util.h src/Util.cpp
        
        src/Program/Program.h src/Program/Program.cpp
        src/Program/Config.h src/Program/Config.cpp
        src/Program/GuiDrawer.h src/Program/GuiDrawer.cpp
        src/Program/MenuBar.h src/Program/MenuBar.cpp
        src/Program/PanelWindow.h src/Program/PanelWindow.cpp
        src/Program/RendererType.h
        src/Program/Sound.h src/Program/Sound.cpp
        
        src/Program/Manager/CameraManager.h src/Program/Manager/CameraManager.cpp
        src/Program/Manager/InputManager.h src/Program/Manager/InputManager.cpp
        src/Program/Manager/PanelManager.h src/Program/Manager/PanelManager.cpp
        
        src/Program/Panel/Panel.h
        src/Program/Panel/ModelPanel.h src/Program/Panel/ModelPanel.cpp
        src/Program/Panel/MotionPanel.h src/Program/Panel/MotionPanel.cpp
        src/Program/Panel/PlaybackPanel.h src/Program/Panel/PlaybackPanel.cpp
        src/Program/Panel/SoundPanel.h src/Program/Panel/SoundPanel.cpp
        
        src/Parser/PmxParser.h src/Parser/PmxParser.cpp
        src/Parser/VmdParser.h src/Parser/VmdParser.cpp
        src/Parser/BinaryReader.h src/Parser/BinaryReader.cpp

        src/Animation/AnimationKeySearch.h
        src/Animation/Bezier.h src/Animation/Bezier.cpp

        src/Animation/Model/Animation.h src/Animation/Model/Animation.cpp
        src/Animation/Model/AnimationBinder.h src/Animation/Model/AnimationBinder.cpp
        src/Animation/Model/AnimationBuilder.h src/Animation/Model/AnimationBuilder.cpp
        src/Animation/Model/AnimationTrackMap.h src/Animation/Model/AnimationTrackMap.cpp
        
        src/Animation/Camera/CameraAnimation.h src/Animation/Camera/CameraAnimation.cpp
        src/Animation/Camera/CameraAnimationBuilder.h src/Animation/Camera/CameraAnimationBuilder.cpp

        src/Model/Model.h src/Model/Model.cpp
        src/Model/ModelAnimator.h src/Model/ModelAnimator.cpp
        src/Model/ModelLoader.h src/Model/ModelLoader.cpp
        src/Model/ModelMorph.h src/Model/ModelMorph.cpp
        src/Model/ModelPose.h src/Model/ModelPose.cpp
        src/Model/ModelSkinning.h src/Model/ModelSkinning.cpp
        
        src/Model/Bone/IkSolver.h src/Model/Bone/IkSolver.cpp
        src/Model/Bone/Node.h src/Model/Bone/Node.cpp
        
        src/Model/Physics/Physics.h src/Model/Physics/Physics.cpp
        src/Model/Physics/RigidBody.h src/Model/Physics/RigidBody.cpp
        src/Model/Physics/Joint.h src/Model/Physics/Joint.cpp
        src/Model/Physics/MotionState.h src/Model/Physics/MotionState.cpp

        src/Viewer/Viewer.h src/Viewer/Viewer.cpp
        src/Viewer/Drawer.h src/Viewer/Drawer.cpp
        src/Viewer/Instance.h src/Viewer/Instance.cpp
        src/Viewer/TextureCache.h
        
        src/Viewer/Glfw/GlfwViewer.h src/Viewer/Glfw/GlfwViewer.cpp
        src/Viewer/Glfw/GlfwDrawer.h src/Viewer/Glfw/GlfwDrawer.cpp
        src/Viewer/Glfw/GlfwTextureCache.h src/Viewer/Glfw/GlfwTextureCache.cpp
        src/Viewer/Glfw/GlfwInstance.h src/Viewer/Glfw/GlfwInstance.cpp
        
        src/Viewer/Dx11/Dx11Viewer.h src/Viewer/Dx11/Dx11Viewer.cpp
        src/Viewer/Dx11/Dx11Drawer.h src/Viewer/Dx11/Dx11Drawer.cpp
        src/Viewer/Dx11/Dx11TextureCache.h src/Viewer/Dx11/Dx11TextureCache.cpp
        src/Viewer/Dx11/Dx11Instance.h src/Viewer/Dx11/Dx11Instance.cpp

        src/Viewer/Vulkan/VulkanViewer.h src/Viewer/Vulkan/VulkanViewer.cpp
        src/Viewer/Vulkan/VulkanDrawer.h src/Viewer/Vulkan/VulkanDrawer.cpp
        src/Viewer/Vulkan/VulkanTextureCache.h src/Viewer/Vulkan/VulkanTextureCache.cpp
        src/Viewer/Vulkan/VulkanInstance.h src/Viewer/Vulkan/VulkanInstance.cpp

        src/Viewer/Glfw/Helper/GlfwShader.h src/Viewer/Glfw/Helper/GlfwShader.cpp
        src/Viewer/Glfw/Helper/GlfwShaderCompiler.h src/Viewer/Glfw/Helper/GlfwShaderCompiler.cpp
        src/Viewer/Dx11/Helper/Dx11Constants.h
        src/Viewer/Dx11/Helper/Dx11DescriptorFactory.h src/Viewer/Dx11/Helper/Dx11DescriptorFactory.cpp
        src/Viewer/Vulkan/Helper/VulkanBuffer.h src/Viewer/Vulkan/Helper/VulkanBuffer.cpp
        src/Viewer/Vulkan/Helper/VulkanCommandBuffer.h src/Viewer/Vulkan/Helper/VulkanCommandBuffer.cpp
        src/Viewer/Vulkan/Helper/VulkanCommandContext.h src/Viewer/Vulkan/Helper/VulkanCommandContext.cpp
        src/Viewer/Vulkan/Helper/VulkanDepthBuffer.h src/Viewer/Vulkan/Helper/VulkanDepthBuffer.cpp
        src/Viewer/Vulkan/Helper/VulkanDevice.h src/Viewer/Vulkan/Helper/VulkanDevice.cpp
        src/Viewer/Vulkan/Helper/VulkanPipeline.h src/Viewer/Vulkan/Helper/VulkanPipeline.cpp
        src/Viewer/Vulkan/Helper/VulkanRenderPass.h src/Viewer/Vulkan/Helper/VulkanRenderPass.cpp
        src/Viewer/Vulkan/Helper/VulkanShaderModule.h src/Viewer/Vulkan/Helper/VulkanShaderModule.cpp
        src/Viewer/Vulkan/Helper/VulkanFrameBuffer.h src/Viewer/Vulkan/Helper/VulkanFrameBuffer.cpp
        src/Viewer/Vulkan/Helper/VulkanSwapChain.h src/Viewer/Vulkan/Helper/VulkanSwapChain.cpp
        src/Viewer/Vulkan/Helper/VulkanSyncObject.h src/Viewer/Vulkan/Helper/VulkanSyncObject.cpp
)

target_link_libraries(PmxMod PRIVATE
        glfw
        glad::glad
        glm::glm

        d3d11
        dxgi
        d3dcompiler
        comctl32
        Vulkan::Vulkan

        BulletDynamics
        BulletCollision
        LinearMath
)
target_include_directories(PmxMod PRIVATE
        ${Stb_INCLUDE_DIR}
        ${MINIAUDIO_INCLUDE_DIR}
)

set(RESOURCE_SRC_DIR "${CMAKE_SOURCE_DIR}/resource")
set(RESOURCE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")
set(VULKAN_SHADER_SRC_DIR "${RESOURCE_SRC_DIR}/shader_Vulkan")
set(VULKAN_SHADER_OUT_DIR "${RESOURCE_OUT_DIR}/shader_Vulkan")
set(VULKAN_SHADER_SOURCES
        model.vert
        model.frag
        edge.vert
        edge.frag
        ground_shadow.vert
        ground_shadow.frag
        debug_triangle.vert
        debug_triangle.frag
)

add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${RESOURCE_SRC_DIR}" "${RESOURCE_OUT_DIR}"
        VERBATIM
)

set(VULKAN_SHADER_OUTPUTS)
foreach(shader ${VULKAN_SHADER_SOURCES})
        set(shader_src "${VULKAN_SHADER_SRC_DIR}/${shader}")
        set(shader_out "${VULKAN_SHADER_OUT_DIR}/${shader}.spv")
        list(APPEND VULKAN_SHADER_OUTPUTS "${shader_out}")
        add_custom_command(
                OUTPUT "${shader_out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${VULKAN_SHADER_OUT_DIR}"
                COMMAND "${GLSLC_EXECUTABLE}" "${shader_src}" -o "${shader_out}"
                DEPENDS "${shader_src}"
                VERBATIM
        )
endforeach()

add_custom_target(CompileVulkanShaders DEPENDS ${VULKAN_SHADER_OUTPUTS})
add_dependencies(CompileVulkanShaders SyncResources)
add_dependencies(PmxMod CompileVulkanShaders)

add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_INSTALLED_DIR}/bin/glfw3.dll"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)
