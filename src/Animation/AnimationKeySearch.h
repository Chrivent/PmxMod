#pragma once

#include <algorithm>

namespace Chrivent {
	class AnimationKeySearch {
	public:
		template <typename Keys>
		static auto FindUpperKey(const Keys& keys, const float t) {
			return std::ranges::upper_bound(keys, t, std::less{}, [](const auto& key) {
				return static_cast<float>(key.frame);
			});
		}
	};
}
