add_library(PmxModViewer STATIC)
add_library(PmxMod::Viewer ALIAS PmxModViewer)

add_library(PmxModDxcCompiler OBJECT
        Shader/DxcShaderCompiler.cpp
)

target_compile_features(PmxModDxcCompiler PRIVATE cxx_std_23)

target_include_directories(PmxModDxcCompiler PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        "$ENV{VULKAN_SDK}/Include"
)

target_sources(PmxModViewer PRIVATE
        Buffer/DynamicBufferRing.h Buffer/DynamicBufferRing.cpp
        Buffer/GlfwDynamicBufferRing.h Buffer/GlfwDynamicBufferRing.cpp
        Buffer/Dx12Buffer.h Buffer/Dx12Buffer.cpp
        Buffer/VulkanBuffer.h Buffer/VulkanBuffer.cpp
        Buffer/VulkanDynamicBufferRing.h Buffer/VulkanDynamicBufferRing.cpp

        Command/Dx12CommandContext.h Command/Dx12CommandContext.cpp
        Command/VulkanCommandBuffer.h Command/VulkanCommandBuffer.cpp
        Command/VulkanCommandContext.h Command/VulkanCommandContext.cpp

        Descriptor/Dx11DescBuilder.h Descriptor/Dx11DescBuilder.cpp
        Descriptor/VulkanDescriptorSet.h Descriptor/VulkanDescriptorSet.cpp

        Device/GraphicsCapabilities.h Device/GraphicsCapabilities.cpp
        Device/Dx12Device.h Device/Dx12Device.cpp
        Device/VulkanDevice.h Device/VulkanDevice.cpp

        Drawer/Drawer.h Drawer/Drawer.cpp
        Drawer/GlfwDrawer.h Drawer/GlfwDrawer.cpp
        Drawer/Dx11Drawer.h Drawer/Dx11Drawer.cpp
        Drawer/Dx12Drawer.h Drawer/Dx12Drawer.cpp
        Drawer/VulkanDrawer.h Drawer/VulkanDrawer.cpp

        Effect/PostProcessInputLayout.h
        Effect/ShaderPackage.h Effect/ShaderPackage.cpp

        Geometry/ViewerGeometry.h Geometry/ViewerGeometry.cpp

        Instance/Instance.h Instance/Instance.cpp
        Instance/GlfwInstance.h Instance/GlfwInstance.cpp
        Instance/Dx11Instance.h Instance/Dx11Instance.cpp
        Instance/Dx12Instance.h Instance/Dx12Instance.cpp
        Instance/VulkanInstance.h Instance/VulkanInstance.cpp

        Memory/VulkanMemory.h Memory/VulkanMemory.cpp

        Pipeline/Dx12Pipeline.h Pipeline/Dx12Pipeline.cpp
        Pipeline/Dx12PipelineBuilder.h Pipeline/Dx12PipelineBuilder.cpp
        Pipeline/VulkanPipeline.h Pipeline/VulkanPipeline.cpp

        PostProcess/PostProcess.h PostProcess/PostProcess.cpp
        PostProcess/GlfwPostProcess.h PostProcess/GlfwPostProcess.cpp
        PostProcess/Dx11PostProcess.h PostProcess/Dx11PostProcess.cpp
        PostProcess/Dx12PostProcess.h PostProcess/Dx12PostProcess.cpp
        PostProcess/VulkanPostProcess.h PostProcess/VulkanPostProcess.cpp

        RenderTarget/Dx12DepthBuffer.h RenderTarget/Dx12DepthBuffer.cpp
        RenderTarget/Dx12MsaaColorBuffer.h RenderTarget/Dx12MsaaColorBuffer.cpp
        RenderTarget/VulkanMsaaColorBuffer.h RenderTarget/VulkanMsaaColorBuffer.cpp
        RenderTarget/VulkanMsaaDepthBuffer.h RenderTarget/VulkanMsaaDepthBuffer.cpp

        Shader/ShaderConstants.h
        Shader/ShaderCompiler.h Shader/ShaderCompiler.cpp
        Shader/DxcShaderCompiler.h
        $<TARGET_OBJECTS:PmxModDxcCompiler>
        Shader/GlfwShader.h Shader/GlfwShader.cpp
        Shader/GlfwShaderCompiler.h Shader/GlfwShaderCompiler.cpp
        Shader/Dx11Shader.h Shader/Dx11Shader.cpp
        Shader/VulkanShaderModule.h Shader/VulkanShaderModule.cpp

        SwapChain/Dx12SwapChain.h SwapChain/Dx12SwapChain.cpp
        SwapChain/VulkanSwapChain.h SwapChain/VulkanSwapChain.cpp

        Synchronization/Dx12Barrier.h Synchronization/Dx12Barrier.cpp
        Synchronization/VulkanSyncObject.h Synchronization/VulkanSyncObject.cpp

        Texture/TextureCache.h
        Texture/GlfwTextureCache.h Texture/GlfwTextureCache.cpp
        Texture/Dx11TextureCache.h Texture/Dx11TextureCache.cpp
        Texture/Dx12TextureCache.h Texture/Dx12TextureCache.cpp
        Texture/VulkanTextureCache.h Texture/VulkanTextureCache.cpp

        Viewer/Viewer.h Viewer/Viewer.cpp
        Viewer/GlfwViewer.h Viewer/GlfwViewer.cpp
        Viewer/Dx11Viewer.h Viewer/Dx11Viewer.cpp
        Viewer/Dx12Viewer.h Viewer/Dx12Viewer.cpp
        Viewer/VulkanViewer.h Viewer/VulkanViewer.cpp
)

target_compile_features(PmxModViewer PUBLIC cxx_std_23)

target_include_directories(PmxModViewer
        PUBLIC ${PROJECT_SOURCE_DIR}/src
        PRIVATE ${Stb_INCLUDE_DIR}
)

target_link_libraries(PmxModViewer
        PUBLIC
        PmxModCore
        glfw
        glad::glad
        glm::glm
        Vulkan::Vulkan
        PRIVATE
        nlohmann_json::nlohmann_json
        d3d11
        d3d12
        dxgi
        d3dcompiler
        ${DXCOMPILER_LIBRARY}
        spirv-cross-core
        spirv-cross-cpp
        spirv-cross-glsl
)
