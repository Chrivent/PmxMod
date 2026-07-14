#pragma once

#include <algorithm>

namespace Chrivent {
	// 정렬된 애니메이션 키에서 현재 프레임을 둘러싼 키 구간을 찾는다.
	class AnimationKeySearch {
	public:
		// 지정한 시간보다 큰 첫 번째 키 프레임을 찾는다.
		template <typename Keys>
		static auto FindUpperKey(const Keys& keys, const float t) {
			return std::ranges::upper_bound(keys, t, std::less{}, [](const auto& key) {
				return key.frame;
			});
		}
	};
}
