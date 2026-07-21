#include "Viewer/Error/GraphicsError.h"

#include <gtest/gtest.h>

namespace Chrivent {
	TEST(GraphicsErrorContract, FormatsApiOperationMessageAndNativeCode) {
		const GraphicsError error = GraphicsError::Create(GraphicsApi::Vulkan,
			GraphicsErrorCode::ResourceCreationFailed, "pipeline 생성", "리소스 생성 실패", -4, true);
		EXPECT_EQ(error.Format(),
			"[Vulkan] pipeline 생성: 리소스 생성 실패 (네이티브 코드: -4)");
	}

	TEST(GraphicsErrorContract, FormatsUnknownApiWithoutOptionalDetails) {
		const GraphicsError error = GraphicsError::Create(
			GraphicsApi::Unknown, GraphicsErrorCode::InvalidState, "렌더러 상태 확인", "");
		EXPECT_EQ(error.Format(), "[Graphics] 렌더러 상태 확인");
	}
}
