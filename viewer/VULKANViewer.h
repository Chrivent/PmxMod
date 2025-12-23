#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#include <map>

struct AppContext;
class VMDCameraAnimation;

bool FindMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& memProps,
                         uint32_t typeBits, vk::MemoryPropertyFlags reqMask, uint32_t* typeIndex);
void SetImageLayout(vk::CommandBuffer cmdBuf, vk::Image image,
		vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout,
		const vk::ImageSubresourceRange& subresourceRange);

struct Vertex {
	glm::vec3	m_position;
	glm::vec3	m_normal;
	glm::vec2	m_uv;
};

struct MMDVertxShaderUB {
	glm::mat4	m_wv;
	glm::mat4	m_wvp;
};

struct MMDFragmentShaderUB {
	glm::vec3	m_diffuse;
	float		m_alpha;
	glm::vec3	m_ambient;
	float		m_dummy1;
	glm::vec3	m_specular;
	float		m_specularPower;
	glm::vec3	m_lightColor;
	float		m_dummy2;
	glm::vec3	m_lightDir;
	float		m_dummy3;
	glm::vec4	m_texMulFactor;
	glm::vec4	m_texAddFactor;
	glm::vec4	m_toonTexMulFactor;
	glm::vec4	m_toonTexAddFactor;
	glm::vec4	m_sphereTexMulFactor;
	glm::vec4	m_sphereTexAddFactor;
	glm::ivec4	m_textureModes;
};

struct MMDEdgeVertexShaderUB {
	glm::mat4	m_wv;
	glm::mat4	m_wvp;
	glm::vec2	m_screenSize;
	float		m_dummy[2];
};

struct MMDEdgeSizeVertexShaderUB {
	float		m_edgeSize;
	float		m_dummy[3];
};

struct MMDEdgeFragmentShaderUB {
	glm::vec4	m_edgeColor;
};

struct MMDGroundShadowVertexShaderUB {
	glm::mat4	m_wvp;
};

struct MMDGroundShadowFragmentShaderUB {
	glm::vec4	m_shadowColor;
};

struct SwapChainImageResource
{
	vk::Image		m_image;
	vk::ImageView	m_imageView;
	vk::Framebuffer	m_framebuffer;
	vk::CommandBuffer	m_cmdBuffer;

	void Clear(AppContext& appContext);
};

struct Buffer
{
	vk::DeviceMemory	m_memory;
	vk::Buffer			m_buffer;
	vk::DeviceSize		m_memorySize = 0;

	bool Setup(
		AppContext&				appContext,
		vk::DeviceSize			memSize,
		vk::BufferUsageFlags	usage,
		vk::MemoryPropertyFlags	memProperty
	);
	void Clear(AppContext& appContext);
};

struct Texture {
	vk::Image		m_image;
	vk::ImageView	m_imageView;
	vk::ImageLayout	m_imageLayout = vk::ImageLayout::eUndefined;
	vk::DeviceMemory	m_memory;
	vk::Format			m_format;
	uint32_t			m_mipLevel;
	bool				m_hasAlpha;

	bool Setup(AppContext& appContext, uint32_t width, uint32_t height, vk::Format format, int mipCount = 1);
	void Clear(AppContext& appContext);
};

struct StagingBuffer {
	vk::DeviceMemory	m_memory;
	vk::Buffer			m_buffer;
	vk::DeviceSize		m_memorySize = 0;
	vk::CommandBuffer	m_copyCommand;
	vk::Fence			m_transferCompleteFence;
	vk::Semaphore		m_transferCompleteSemaphore;
	vk::Semaphore		m_waitSemaphore;

	bool Setup(AppContext& appContext, vk::DeviceSize size);
	void Clear(AppContext& appContext);
	void Wait(AppContext& appContext);
	bool CopyBuffer(AppContext& appContext, vk::Buffer destBuffer, vk::DeviceSize size);
	bool CopyImage(
		AppContext& appContext,
		vk::Image destImage,
		vk::ImageLayout imageLayout,
		uint32_t regionCount,
		vk::BufferImageCopy* regions
	);
};

struct AppContext
{
	enum class MMDRenderType {
		AlphaBlend_FrontFace,
		AlphaBlend_BothFace,
		MaxTypes
	};

	struct FrameSyncData {
		vk::Fence		m_fence;
		vk::Semaphore	m_presentCompleteSemaphore;
		vk::Semaphore	m_renderCompleteSemaphore;
	};

	std::string m_resourceDir;
	std::string	m_shaderDir;
	std::string	m_mmdDir;
	glm::mat4	m_viewMat;
	glm::mat4	m_projMat;
	int	m_screenWidth = 0;
	int	m_screenHeight = 0;
	glm::vec3	m_lightColor = glm::vec3(1, 1, 1);
	glm::vec3	m_lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
	float	m_elapsed = 0.0f;
	float	m_animTime = 0.0f;
	std::unique_ptr<VMDCameraAnimation>	m_vmdCameraAnim;
	vk::Instance		m_vkInst;
	vk::SurfaceKHR		m_surface;
	vk::PhysicalDevice	m_gpu;
	vk::Device			m_device;
	vk::PhysicalDeviceMemoryProperties	m_memProperties;
	uint32_t	m_graphicsQueueFamilyIndex;
	uint32_t	m_presentQueueFamilyIndex;
	vk::Format	m_colorFormat = vk::Format::eUndefined;
	vk::Format	m_depthFormat = vk::Format::eUndefined;
	vk::SampleCountFlagBits	m_msaaSampleCount = vk::SampleCountFlagBits::e4;
	std::vector<FrameSyncData>	m_frameSyncDatas;
	uint32_t					m_frameIndex = 0;
	vk::SwapchainKHR					m_swapChain;
	std::vector<SwapChainImageResource>	m_swapChainImageResources;
	vk::Image							m_depthImage;
	vk::DeviceMemory					m_depthMem;
	vk::ImageView						m_depthImageView;
	vk::Image							m_msaaColorImage;
	vk::DeviceMemory					m_msaaColorMem;
	vk::ImageView						m_msaaColorImageView;
	vk::Image							m_msaaDepthImage;
	vk::DeviceMemory					m_msaaDepthMem;
	vk::ImageView						m_msaaDepthImageView;
	vk::RenderPass	m_renderPass;
	vk::PipelineCache	m_pipelineCache;
	vk::DescriptorSetLayout	m_mmdDescriptorSetLayout;
	vk::PipelineLayout	m_mmdPipelineLayout;
	vk::Pipeline		m_mmdPipelines[static_cast<int>(MMDRenderType::MaxTypes)];
	vk::ShaderModule	m_mmdVSModule;
	vk::ShaderModule	m_mmdFSModule;
	vk::DescriptorSetLayout	m_mmdEdgeDescriptorSetLayout;
	vk::PipelineLayout	m_mmdEdgePipelineLayout;
	vk::Pipeline		m_mmdEdgePipeline;
	vk::ShaderModule	m_mmdEdgeVSModule;
	vk::ShaderModule	m_mmdEdgeFSModule;
	vk::DescriptorSetLayout	m_mmdGroundShadowDescriptorSetLayout;
	vk::PipelineLayout	m_mmdGroundShadowPipelineLayout;
	vk::Pipeline		m_mmdGroundShadowPipeline;
	vk::ShaderModule	m_mmdGroundShadowVSModule;
	vk::ShaderModule	m_mmdGroundShadowFSModule;
	const uint32_t DefaultImageCount = 3;
	uint32_t	m_imageCount;
	uint32_t	m_imageIndex = 0;
	vk::CommandPool		m_commandPool;
	vk::CommandPool		m_transferCommandPool;
	vk::Queue			m_graphicsQueue;
	std::vector<std::unique_ptr<StagingBuffer>>	m_stagingBuffers;
	using TextureUPtr = std::unique_ptr<Texture>;
	std::map<std::string, TextureUPtr>	m_textures;
	TextureUPtr	m_defaultTexture;
	vk::Sampler	m_defaultSampler;

	bool Setup(vk::Instance inst, vk::SurfaceKHR surface, vk::PhysicalDevice gpu, vk::Device device);
	void Destory();
	bool Prepare();
	bool PrepareCommandPool();
	bool PrepareBuffer();
	bool PrepareSyncObjects();
	bool PrepareRenderPass();
	bool PrepareFramebuffer();
	bool PreparePipelineCache();
	bool PrepareMMDPipeline();
	bool PrepareMMDEdgePipeline();
	bool PrepareMMDGroundShadowPipeline();
	bool PrepareDefaultTexture();
	bool Resize();
	bool GetStagingBuffer(vk::DeviceSize memSize, StagingBuffer** outBuf);
	void WaitAllStagingBuffer();
	bool GetTexture(const std::string& texturePath, Texture** outTex);
};
