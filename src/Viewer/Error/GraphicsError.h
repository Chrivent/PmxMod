#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace Chrivent {
	// 그래픽 오류가 발생한 렌더링 API를 식별한다.
	enum class GraphicsApi {
		Unknown,
		OpenGl,
		DirectX11,
		DirectX12,
		Vulkan
	};

	// 그래픽 실패의 성격을 호출자가 분기할 수 있는 안정된 범주로 구분한다.
	enum class GraphicsErrorCode {
		InvalidArgument,
		InvalidState,
		UnsupportedFeature,
		InitializationFailed,
		ResourceCreationFailed,
		EffectConfigurationFailed,
		CommandRecordingFailed,
		CommandSubmissionFailed,
		PresentationFailed,
		SynchronizationFailed,
		ContractViolation
	};

	// 렌더링 API 경계에서 발생한 실패 원인과 네이티브 결과 코드를 보관한다.
	struct GraphicsError {
		GraphicsApi api = GraphicsApi::Unknown;
		GraphicsErrorCode code = GraphicsErrorCode::InvalidState;
		std::string operation;
		std::string message;
		int64_t nativeCode = 0;
		bool hasNativeCode = false;

		// 그래픽 작업의 성공값 또는 현재 구조화된 오류를 반환하는 형식이다.
		template <typename T>
		using Result = std::expected<T, GraphicsError>;

		// API 식별자와 작업 문맥으로 구조화된 그래픽 오류를 생성한다.
		static GraphicsError Create(GraphicsApi api, GraphicsErrorCode code,
			std::string operation, std::string message,
			int64_t nativeCode = 0, bool hasNativeCode = false);
		// 프로그램 경계에서 한 번 출력할 진단 문자열을 생성한다.
		std::string Format() const;
	};
}
