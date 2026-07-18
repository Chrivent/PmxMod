#include "Viewer/Error/GraphicsError.h"

#include <utility>

namespace Chrivent {
	// 그래픽 API 식별자를 사용자 진단에 사용할 이름으로 변환한다.
	static const char* ResolveGraphicsApiName(const GraphicsApi api) {
		switch (api) {
			case GraphicsApi::OpenGl:
				return "OpenGL";
			case GraphicsApi::DirectX11:
				return "DirectX 11";
			case GraphicsApi::DirectX12:
				return "DirectX 12";
			case GraphicsApi::Vulkan:
				return "Vulkan";
			case GraphicsApi::Unknown:
				return "Graphics";
		}
		return "Graphics";
	}

	std::string GraphicsError::Format() const {
		std::string formatted = "[";
		formatted += ResolveGraphicsApiName(api);
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

	GraphicsError MakeGraphicsError(const GraphicsApi api, const GraphicsErrorCode code,
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
}
