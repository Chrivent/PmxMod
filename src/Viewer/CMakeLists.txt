add_library(PmxModViewer STATIC)
add_library(PmxMod::Viewer ALIAS PmxModViewer)

add_library(PmxModModernHlslCompiler OBJECT
        Shader/ModernHlslCompiler.cpp
)

target_compile_features(PmxModModernHlslCompiler PRIVATE cxx_std_23)

target_include_directories(PmxModModernHlslCompiler PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        "$ENV{VULKAN_SDK}/Include"
)

target_sources(PmxModViewer PRIVATE
        Buffer/DynamicBufferRing.h Buffer/DynamicBufferRing.cpp
        Buffer/OpenGlDynamicBufferRing.h Buffer/OpenGlDynamicBufferRing.cpp
        Buffer/Dx12Buffer.h Buffer/Dx12Buffer.cpp
        Buffer/VulkanBuffer.h Buffer/VulkanBuffer.cpp
        Buffer/VulkanDynamicBufferRing.h Buffer/VulkanDynamicBufferRing.cpp

        Command/Dx12CommandContext.h Command/Dx12CommandContext.cpp
        Command/Dx12UploadContext.h Command/Dx12UploadContext.cpp
        Command/VulkanCommandBuffer.h Command/VulkanCommandBuffer.cpp
        Command/VulkanCommandContext.h Command/VulkanCommandContext.cpp
        Command/VulkanUploadContext.h Command/VulkanUploadContext.cpp

        Descriptor/Dx11DescBuilder.h Descriptor/Dx11DescBuilder.cpp
        Descriptor/VulkanDescriptorSet.h Descriptor/VulkanDescriptorSet.cpp
        Descriptor/VulkanPostProcessDescriptors.h Descriptor/VulkanPostProcessDescriptors.cpp

        Device/GraphicsCapabilities.h Device/GraphicsCapabilities.cpp
        Device/Dx11Device.h Device/Dx11Device.cpp
        Device/Dx12Device.h Device/Dx12Device.cpp
        Device/VulkanDevice.h Device/VulkanDevice.cpp

        Error/GraphicsError.h Error/GraphicsError.cpp

		Drawer/SceneDrawState.h
		Drawer/Drawer.h Drawer/Drawer.cpp
		Drawer/OpenGlDrawer.h Drawer/OpenGlDrawer.cpp
		Drawer/Dx11Drawer.h Drawer/Dx11Drawer.cpp
		Drawer/Dx12Drawer.h Drawer/Dx12Drawer.cpp
		Drawer/VulkanDrawer.h Drawer/VulkanDrawer.cpp

		DrawContext/OpenGlDrawContext.h DrawContext/OpenGlDrawContext.cpp
		DrawContext/Dx11DrawContext.h DrawContext/Dx11DrawContext.cpp
		DrawContext/Dx12DrawContext.h DrawContext/Dx12DrawContext.cpp
		DrawContext/VulkanDrawContext.h DrawContext/VulkanDrawContext.cpp

		DrawResource/OpenGlModelResources.h
		DrawResource/Dx11ModelResources.h
		DrawResource/Dx12ModelResources.h
		DrawResource/VulkanModelResources.h

        Geometry/ViewerGeometry.h Geometry/ViewerGeometry.cpp

        Instance/Instance.h Instance/Instance.cpp
        Instance/OpenGlInstance.h Instance/OpenGlInstance.cpp
        Instance/Dx11Instance.h Instance/Dx11Instance.cpp
        Instance/Dx12Instance.h Instance/Dx12Instance.cpp
        Instance/VulkanInstance.h Instance/VulkanInstance.cpp

        Memory/VulkanMemory.h Memory/VulkanMemory.cpp

        Pipeline/Dx11Pipeline.h Pipeline/Dx11Pipeline.cpp
		Pipeline/OpenGlPipeline.h Pipeline/OpenGlPipeline.cpp
        Pipeline/Dx12Pipeline.h Pipeline/Dx12Pipeline.cpp
        Pipeline/Dx12PipelineBuilder.h Pipeline/Dx12PipelineBuilder.cpp
        Pipeline/VulkanPipeline.h Pipeline/VulkanPipeline.cpp
		Pipeline/VulkanShaderStageBuilder.h Pipeline/VulkanShaderStageBuilder.cpp

        PostProcess/PostProcess.h PostProcess/PostProcess.cpp
        PostProcess/PostProcessInputLayout.h
        PostProcess/PostProcessRuntimeContract.h
        PostProcess/OpenGlPostProcess.h PostProcess/OpenGlPostProcess.cpp
        PostProcess/Dx11PostProcess.h PostProcess/Dx11PostProcess.cpp
        PostProcess/Dx12PostProcess.h PostProcess/Dx12PostProcess.cpp
        PostProcess/Dx12PostProcessPipelines.h PostProcess/Dx12PostProcessPipelines.cpp
        PostProcess/VulkanPostProcess.h PostProcess/VulkanPostProcess.cpp
        PostProcess/VulkanPostProcessPipelines.h PostProcess/VulkanPostProcessPipelines.cpp

        RenderTarget/Dx11RenderTargets.h RenderTarget/Dx11RenderTargets.cpp
        RenderTarget/Dx12DepthBuffer.h RenderTarget/Dx12DepthBuffer.cpp
        RenderTarget/Dx12MsaaColorBuffer.h RenderTarget/Dx12MsaaColorBuffer.cpp
        RenderTarget/Dx12PostProcessTarget.h RenderTarget/Dx12PostProcessTarget.cpp
		RenderTarget/VulkanMsaaColorBuffer.h RenderTarget/VulkanMsaaColorBuffer.cpp
		RenderTarget/VulkanMsaaDepthBuffer.h RenderTarget/VulkanMsaaDepthBuffer.cpp
		RenderTarget/VulkanPostProcessTarget.h RenderTarget/VulkanPostProcessTarget.cpp

        Shader/ShaderConstants.h
        Shader/ShaderProgramDefinition.h
        Shader/InternalShaderCatalog.h Shader/InternalShaderCatalog.cpp
        Shader/SceneShaderInputLayout.h
        Shader/SceneShaderRuntimeContract.h
        Shader/LegacyHlslCompiler.h Shader/LegacyHlslCompiler.cpp
        Shader/ModernHlslCompiler.h
        Shader/SpirvBindingLayout.h
        $<TARGET_OBJECTS:PmxModModernHlslCompiler>
        Shader/OpenGlShader.h Shader/OpenGlShader.cpp
        Shader/Dx11Shader.h Shader/Dx11Shader.cpp
        Shader/VulkanShaderModule.h Shader/VulkanShaderModule.cpp

        SwapChain/Dx12SwapChain.h SwapChain/Dx12SwapChain.cpp
        SwapChain/VulkanSwapChain.h SwapChain/VulkanSwapChain.cpp

        Synchronization/Dx12Barrier.h Synchronization/Dx12Barrier.cpp
		Synchronization/FrameBuffering.h
        Synchronization/VulkanSyncObject.h Synchronization/VulkanSyncObject.cpp

        Texture/TextureCache.h Texture/TextureCache.cpp
        Texture/OpenGlTextureCache.h Texture/OpenGlTextureCache.cpp
        Texture/Dx11TextureCache.h Texture/Dx11TextureCache.cpp
        Texture/Dx12TextureCache.h Texture/Dx12TextureCache.cpp
        Texture/VulkanTextureCache.h Texture/VulkanTextureCache.cpp

        Viewer/Viewer.h Viewer/Viewer.cpp
        Viewer/OpenGlViewer.h Viewer/OpenGlViewer.cpp
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
        d3d11
        d3d12
        dxgi
        d3dcompiler
        ${DXCOMPILER_LIBRARY}
        spirv-cross-core
        spirv-cross-cpp
        spirv-cross-glsl
)
