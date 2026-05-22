#pragma once

#include "../Viewer.h"
#include "VulkanTextureCache.h"

namespace Chrivent {
	struct VulkanMaterial : ViewerMaterial {
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};

		explicit VulkanMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};
	
	struct VulkanViewerInfo : ViewerInfo {
	};

	class VulkanViewer : public Viewer {
		VulkanTextureCache textureCache;

	public:
		VulkanViewer();
		~VulkanViewer() override = default;

		VulkanViewerInfo& GetVulkanInfo() { return static_cast<VulkanViewerInfo&>(GetInfo()); }
		const VulkanViewerInfo& GetVulkanInfo() const { return static_cast<const VulkanViewerInfo&>(GetInfo()); }

		// Vulkan ?뚮뜑留곸뿉 ?꾩슂??GLFW ?덈룄???뚰듃瑜??ㅼ젙?쒕떎.
		void ConfigureGlfwHints() override;
		// Vulkan ?뚮뜑??由ъ냼?ㅻ? 珥덇린?뷀븳??
		bool Setup() override;
		// 李??ш린??留욎떠 Vulkan ?ㅼ솑泥댁씤怨??뚮뜑 ?源껋쓣 ?ъ깮?깊븳??
		bool Resize() override;
		// Vulkan ?꾨젅???뚮뜑留곸쓣 ?쒖옉?쒕떎.
		void BeginFrame() override;
		// Vulkan ?꾨젅?꾩쓣 ?쒖텧?섍퀬 ?붾㈃???쒖떆?쒕떎.
		bool EndFrame() override;
		// Vulkan 紐⑤뜽 ?몄뒪?댁뒪瑜??앹꽦?쒕떎.
		std::unique_ptr<Instance> CreateInstance() const override;
		// ?띿뒪泥섎? 罹먯떆?먯꽌 李얘굅???뚯씪?먯꽌 濡쒕뱶??Vulkan ?띿뒪泥섎줈 諛섑솚?쒕떎.
		VulkanTexture LoadTexture(const std::filesystem::path& texturePath);
	};
}
