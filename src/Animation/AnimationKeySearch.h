#pragma once

#include <algorithm>

namespace Chrivent {
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
