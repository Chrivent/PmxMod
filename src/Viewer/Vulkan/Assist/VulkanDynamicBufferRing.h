#pragma once

#include "../../Assist/Glsl/GlslDynamicBufferRing.h"
#include "../Helper/VulkanBuffer.h"

namespace Chrivent {
	class VulkanDynamicBufferRing : public GlslDynamicBufferRing {
		VulkanBuffer buffer;
		size_t mappedOffset = 0;

	public:
		~VulkanDynamicBufferRing() override = default;
		
		VulkanBuffer& GetBuffer() { return buffer; }
		const VulkanBuffer& GetBuffer() const { return buffer; }
		size_t GetMappedOffset() const { return mappedOffset; }

		// Vulkan 업로드 링 버퍼를 생성한다.
		bool Setup(size_t bufferSize, std::string& outError) override;
		void Clear() override;
		std::optional<GlslUploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) override;
	};
}
