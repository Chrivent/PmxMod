#include "Viewer/Buffer/DynamicBufferRing.h"

#include <gtest/gtest.h>

namespace Chrivent {
	// API 리소스 없이 공통 동적 버퍼 할당 계약을 노출한다.
	class DynamicBufferRingTestAdapter final : public DynamicBufferRing {
	public:
		// 지정한 용량 안에서 정렬된 업로드 구간을 예약한다.
		DynamicBufferError::Result<UploadSlice> Allocate(const size_t size, const size_t alignment) {
			return AllocateSlice(size, alignment, capacity, 0, "테스트 동적 버퍼 용량이 부족합니다.");
		}
	};

	TEST(DynamicBufferRingContract, RejectsZeroCapacity) {
		DynamicBufferRingTestAdapter ring;
		const auto result = ring.Setup(0);
		ASSERT_FALSE(result.has_value());
		EXPECT_FALSE(result.error().message.empty());
	}

	TEST(DynamicBufferRingContract, AlignsSequentialAllocations) {
		DynamicBufferRingTestAdapter ring;
		ASSERT_TRUE(ring.Setup(64).has_value());
		const auto first = ring.Allocate(3, 4);
		const auto second = ring.Allocate(4, 8);
		ASSERT_TRUE(first.has_value());
		ASSERT_TRUE(second.has_value());
		EXPECT_EQ(first->offset, 0);
		EXPECT_EQ(second->offset, 8);
	}

	TEST(DynamicBufferRingContract, ReportsCapacityFailure) {
		DynamicBufferRingTestAdapter ring;
		ASSERT_TRUE(ring.Setup(8).has_value());
		ASSERT_TRUE(ring.Allocate(8, 1).has_value());
		const auto result = ring.Allocate(1, 1);
		ASSERT_FALSE(result.has_value());
		EXPECT_EQ(result.error().message, "테스트 동적 버퍼 용량이 부족합니다.");
	}

	TEST(DynamicBufferRingContract, ResetsAllocationOffsetForNewFrame) {
		DynamicBufferRingTestAdapter ring;
		ASSERT_TRUE(ring.Setup(16).has_value());
		ASSERT_TRUE(ring.Allocate(8, 1).has_value());
		ring.BeginFrame(1);
		const auto result = ring.Allocate(8, 1);
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->offset, 0);
	}
}
