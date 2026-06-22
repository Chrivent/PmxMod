add_library(PmxModViewer STATIC)
add_library(PmxMod::Viewer ALIAS PmxModViewer)

target_sources(PmxModViewer PRIVATE
        Viewer.h Viewer.cpp
        Drawer.h Drawer.cpp
        Instance.h Instance.cpp
        ShaderConstants.h
        TextureCache.h
        ViewerGeometry.h ViewerGeometry.cpp

        Glsl/GlslPreprocessor.h Glsl/GlslPreprocessor.cpp
        Glsl/GlslShaderConstants.h
        Glsl/GlslDynamicBufferRing.h Glsl/GlslDynamicBufferRing.cpp

        Glsl/Glfw/GlfwViewer.h Glsl/Glfw/GlfwViewer.cpp
        Glsl/Glfw/GlfwDrawer.h Glsl/Glfw/GlfwDrawer.cpp
        Glsl/Glfw/GlfwTextureCache.h Glsl/Glfw/GlfwTextureCache.cpp
        Glsl/Glfw/GlfwInstance.h Glsl/Glfw/GlfwInstance.cpp
        Glsl/Glfw/GlfwDynamicBufferRing.h Glsl/Glfw/GlfwDynamicBufferRing.cpp
        Glsl/Glfw/Helper/GlfwShader.h Glsl/Glfw/Helper/GlfwShader.cpp
        Glsl/Glfw/Helper/GlfwShaderCompiler.h Glsl/Glfw/Helper/GlfwShaderCompiler.cpp

        Glsl/Vulkan/VulkanViewer.h Glsl/Vulkan/VulkanViewer.cpp
        Glsl/Vulkan/VulkanDrawer.h Glsl/Vulkan/VulkanDrawer.cpp
        Glsl/Vulkan/VulkanTextureCache.h Glsl/Vulkan/VulkanTextureCache.cpp
        Glsl/Vulkan/VulkanInstance.h Glsl/Vulkan/VulkanInstance.cpp
        Glsl/Vulkan/VulkanDynamicBufferRing.h Glsl/Vulkan/VulkanDynamicBufferRing.cpp
        Glsl/Vulkan/Helper/VulkanBuffer.h Glsl/Vulkan/Helper/VulkanBuffer.cpp
        Glsl/Vulkan/Helper/VulkanMsaaColorBuffer.h Glsl/Vulkan/Helper/VulkanMsaaColorBuffer.cpp
        Glsl/Vulkan/Helper/VulkanCommandBuffer.h Glsl/Vulkan/Helper/VulkanCommandBuffer.cpp
        Glsl/Vulkan/Helper/VulkanCommandContext.h Glsl/Vulkan/Helper/VulkanCommandContext.cpp
        Glsl/Vulkan/Helper/VulkanMsaaDepthBuffer.h Glsl/Vulkan/Helper/VulkanMsaaDepthBuffer.cpp
        Glsl/Vulkan/Helper/VulkanDescriptorSet.h Glsl/Vulkan/Helper/VulkanDescriptorSet.cpp
        Glsl/Vulkan/Helper/VulkanDevice.h Glsl/Vulkan/Helper/VulkanDevice.cpp
        Glsl/Vulkan/Helper/VulkanMemory.h Glsl/Vulkan/Helper/VulkanMemory.cpp
        Glsl/Vulkan/Helper/VulkanPipeline.h Glsl/Vulkan/Helper/VulkanPipeline.cpp
        Glsl/Vulkan/Helper/VulkanRenderPass.h Glsl/Vulkan/Helper/VulkanRenderPass.cpp
        Glsl/Vulkan/Helper/VulkanShaderCompiler.h Glsl/Vulkan/Helper/VulkanShaderCompiler.cpp
        Glsl/Vulkan/Helper/VulkanShaderModule.h Glsl/Vulkan/Helper/VulkanShaderModule.cpp
        Glsl/Vulkan/Helper/VulkanFrameBuffer.h Glsl/Vulkan/Helper/VulkanFrameBuffer.cpp
        Glsl/Vulkan/Helper/VulkanSwapChain.h Glsl/Vulkan/Helper/VulkanSwapChain.cpp
        Glsl/Vulkan/Helper/VulkanSyncObject.h Glsl/Vulkan/Helper/VulkanSyncObject.cpp

        Hlsl/HlslShaderConstants.h
        Hlsl/HlslShaderCompiler.h Hlsl/HlslShaderCompiler.cpp

        Hlsl/Dx11/Dx11Viewer.h Hlsl/Dx11/Dx11Viewer.cpp
        Hlsl/Dx11/Dx11Drawer.h Hlsl/Dx11/Dx11Drawer.cpp
        Hlsl/Dx11/Dx11TextureCache.h Hlsl/Dx11/Dx11TextureCache.cpp
        Hlsl/Dx11/Dx11Instance.h Hlsl/Dx11/Dx11Instance.cpp
        Hlsl/Dx11/Helper/Dx11DescBuilder.h Hlsl/Dx11/Helper/Dx11DescBuilder.cpp
        Hlsl/Dx11/Helper/Dx11Shader.h Hlsl/Dx11/Helper/Dx11Shader.cpp

        Hlsl/Dx12/Dx12Viewer.h Hlsl/Dx12/Dx12Viewer.cpp
        Hlsl/Dx12/Dx12Drawer.h Hlsl/Dx12/Dx12Drawer.cpp
        Hlsl/Dx12/Dx12TextureCache.h Hlsl/Dx12/Dx12TextureCache.cpp
        Hlsl/Dx12/Dx12Instance.h Hlsl/Dx12/Dx12Instance.cpp
        Hlsl/Dx12/Helper/Dx12Buffer.h Hlsl/Dx12/Helper/Dx12Buffer.cpp
        Hlsl/Dx12/Helper/Dx12CommandContext.h Hlsl/Dx12/Helper/Dx12CommandContext.cpp
        Hlsl/Dx12/Helper/Dx12DepthBuffer.h Hlsl/Dx12/Helper/Dx12DepthBuffer.cpp
        Hlsl/Dx12/Helper/Dx12Device.h Hlsl/Dx12/Helper/Dx12Device.cpp
        Hlsl/Dx12/Helper/Dx12MsaaColorBuffer.h Hlsl/Dx12/Helper/Dx12MsaaColorBuffer.cpp
        Hlsl/Dx12/Helper/Dx12Pipeline.h Hlsl/Dx12/Helper/Dx12Pipeline.cpp
        Hlsl/Dx12/Helper/Dx12SwapChain.h Hlsl/Dx12/Helper/Dx12SwapChain.cpp
)

target_compile_features(PmxModViewer PUBLIC cxx_std_23)

target_include_directories(PmxModViewer
        PUBLIC ${PROJECT_SOURCE_DIR}/src
        PRIVATE ${Stb_INCLUDE_DIR}
)

target_link_libraries(PmxModViewer
        PUBLIC
        PmxModCore
        PmxModShader
        glfw
        glad::glad
        glm::glm
        Vulkan::Vulkan
        PRIVATE
        d3d11
        d3d12
        dxgi
        d3dcompiler
        ${SHADERC_LIBRARY}
)
