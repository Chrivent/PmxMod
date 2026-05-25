#pragma once

#include "../../Assist/Glsl/GlslDynamicBufferRing.h"
#include "../Helper/VulkanBuffer.h"

namespace Chrivent {
	class VulkanDynamicBufferRing : public GlslDynamicBufferRing {
		VulkanBuffer buffer;

	public:
		~VulkanDynamicBufferRing() override = default;
		
		VulkanBuffer& GetBuffer() { return buffer; }
		const VulkanBuffer& GetBuffer() const { return buffer; }

		// Vulkan 업로드 링 버퍼를 생성한다.
		bool Setup(const VulkanDeviceInfo& deviceInfo, size_t bufferSize, std::string& outError);
		void Clear() override;
		std::optional<GlslUploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) override;
		bool Write(const GlslUploadSlice& slice, const void* data, std::string& outError) const;
	};
}
