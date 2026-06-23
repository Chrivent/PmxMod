add_library(PmxModViewer STATIC)
add_library(PmxMod::Viewer ALIAS PmxModViewer)

target_sources(PmxModViewer PRIVATE
        Viewer.h Viewer.cpp
        Drawer.h Drawer.cpp
        Instance.h Instance.cpp
        TextureCache.h
        ViewerGeometry.h ViewerGeometry.cpp
        DynamicBufferRing.h DynamicBufferRing.cpp
        Shader/ShaderConstants.h
        Shader/ShaderCompiler.h Shader/ShaderCompiler.cpp
        Shader/DxcShaderCompiler.h Shader/DxcShaderCompiler.cpp

        Glfw/GlfwViewer.h Glfw/GlfwViewer.cpp
        Glfw/GlfwDrawer.h Glfw/GlfwDrawer.cpp
        Glfw/GlfwTextureCache.h Glfw/GlfwTextureCache.cpp
        Glfw/GlfwInstance.h Glfw/GlfwInstance.cpp
        Glfw/GlfwDynamicBufferRing.h Glfw/GlfwDynamicBufferRing.cpp
        Glfw/Helper/GlfwShader.h Glfw/Helper/GlfwShader.cpp
        Glfw/Helper/GlfwShaderCompiler.h Glfw/Helper/GlfwShaderCompiler.cpp

        Vulkan/VulkanViewer.h Vulkan/VulkanViewer.cpp
        Vulkan/VulkanDrawer.h Vulkan/VulkanDrawer.cpp
        Vulkan/VulkanTextureCache.h Vulkan/VulkanTextureCache.cpp
        Vulkan/VulkanInstance.h Vulkan/VulkanInstance.cpp
        Vulkan/VulkanDynamicBufferRing.h Vulkan/VulkanDynamicBufferRing.cpp
        Vulkan/Helper/VulkanBuffer.h Vulkan/Helper/VulkanBuffer.cpp
        Vulkan/Helper/VulkanMsaaColorBuffer.h Vulkan/Helper/VulkanMsaaColorBuffer.cpp
        Vulkan/Helper/VulkanCommandBuffer.h Vulkan/Helper/VulkanCommandBuffer.cpp
        Vulkan/Helper/VulkanCommandContext.h Vulkan/Helper/VulkanCommandContext.cpp
        Vulkan/Helper/VulkanMsaaDepthBuffer.h Vulkan/Helper/VulkanMsaaDepthBuffer.cpp
        Vulkan/Helper/VulkanDescriptorSet.h Vulkan/Helper/VulkanDescriptorSet.cpp
        Vulkan/Helper/VulkanDevice.h Vulkan/Helper/VulkanDevice.cpp
        Vulkan/Helper/VulkanMemory.h Vulkan/Helper/VulkanMemory.cpp
        Vulkan/Helper/VulkanPipeline.h Vulkan/Helper/VulkanPipeline.cpp
        Vulkan/Helper/VulkanRenderPass.h Vulkan/Helper/VulkanRenderPass.cpp
        Vulkan/Helper/VulkanShaderModule.h Vulkan/Helper/VulkanShaderModule.cpp
        Vulkan/Helper/VulkanFrameBuffer.h Vulkan/Helper/VulkanFrameBuffer.cpp
        Vulkan/Helper/VulkanSwapChain.h Vulkan/Helper/VulkanSwapChain.cpp
        Vulkan/Helper/VulkanSyncObject.h Vulkan/Helper/VulkanSyncObject.cpp

        Dx11/Dx11Viewer.h Dx11/Dx11Viewer.cpp
        Dx11/Dx11Drawer.h Dx11/Dx11Drawer.cpp
        Dx11/Dx11TextureCache.h Dx11/Dx11TextureCache.cpp
        Dx11/Dx11Instance.h Dx11/Dx11Instance.cpp
        Dx11/Helper/Dx11DescBuilder.h Dx11/Helper/Dx11DescBuilder.cpp
        Dx11/Helper/Dx11Shader.h Dx11/Helper/Dx11Shader.cpp

        Dx12/Dx12Viewer.h Dx12/Dx12Viewer.cpp
        Dx12/Dx12Drawer.h Dx12/Dx12Drawer.cpp
        Dx12/Dx12TextureCache.h Dx12/Dx12TextureCache.cpp
        Dx12/Dx12Instance.h Dx12/Dx12Instance.cpp
        Dx12/Helper/Dx12Buffer.h Dx12/Helper/Dx12Buffer.cpp
        Dx12/Helper/Dx12CommandContext.h Dx12/Helper/Dx12CommandContext.cpp
        Dx12/Helper/Dx12DepthBuffer.h Dx12/Helper/Dx12DepthBuffer.cpp
        Dx12/Helper/Dx12Device.h Dx12/Helper/Dx12Device.cpp
        Dx12/Helper/Dx12MsaaColorBuffer.h Dx12/Helper/Dx12MsaaColorBuffer.cpp
        Dx12/Helper/Dx12Pipeline.h Dx12/Helper/Dx12Pipeline.cpp
        Dx12/Helper/Dx12SwapChain.h Dx12/Helper/Dx12SwapChain.cpp
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
        ${DXCOMPILER_LIBRARY}
        ${SPIRV_CROSS_CORE_LIBRARY}
        ${SPIRV_CROSS_LIBRARY}
)

target_include_directories(PmxModViewer PRIVATE "$ENV{VULKAN_SDK}/Include")
