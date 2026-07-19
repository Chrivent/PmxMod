#pragma once

#include <limits>

namespace Chrivent {
	// GPU 버퍼 크기 계산에서 덧셈, 곱셈과 정렬 overflow를 검사한다.
	class BufferSize {
	public:
		// 두 크기를 더할 수 있으면 결과를 기록한다.
		static bool TryAdd(const size_t left, const size_t right, size_t& result) {
			if (left > std::numeric_limits<size_t>::max() - right)
				return false;
			result = left + right;
			return true;
		}

		// 두 크기를 곱할 수 있으면 결과를 기록한다.
		static bool TryMultiply(const size_t left, const size_t right, size_t& result) {
			if (left != 0 && right > std::numeric_limits<size_t>::max() / left)
				return false;
			result = left * right;
			return true;
		}

		// 지정한 정렬 단위에 맞춰 올림한 크기를 기록한다.
		static bool TryAlignUp(const size_t value, const size_t alignment, size_t& result) {
			if (alignment <= 1) {
				result = value;
				return true;
			}
			const size_t remainder = value % alignment;
			if (remainder == 0) {
				result = value;
				return true;
			}
			return TryAdd(value, alignment - remainder, result);
		}
	};
}
