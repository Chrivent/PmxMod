#include "Viewer/Error/GraphicsError.h"

#include <utility>

namespace Chrivent {
	GraphicsError GraphicsError::Create(const GraphicsApi api, const GraphicsErrorCode code,
		std::string operation, std::string message,
		const int64_t nativeCode, const bool hasNativeCode) {
		return {
			.api = api,
			.code = code,
			.operation = std::move(operation),
			.message = std::move(message),
			.nativeCode = nativeCode,
			.hasNativeCode = hasNativeCode
		};
	}

	std::string GraphicsError::Format() const {
		auto apiName = "Graphics";
		switch (api) {
		case GraphicsApi::OpenGl:
			apiName = "OpenGL";
			break;
		case GraphicsApi::DirectX11:
			apiName = "DirectX 11";
			break;
		case GraphicsApi::DirectX12:
			apiName = "DirectX 12";
			break;
		case GraphicsApi::Vulkan:
			apiName = "Vulkan";
			break;
		case GraphicsApi::Unknown:
			break;
		}
		std::string formatted = "[";
		formatted += apiName;
		formatted += "] ";
		formatted += operation;
		if (!message.empty()) {
			formatted += ": ";
			formatted += message;
		}
		if (hasNativeCode) {
			formatted += " (네이티브 코드: ";
			formatted += std::to_string(nativeCode);
			formatted += ')';
		}
		return formatted;
	}
}
