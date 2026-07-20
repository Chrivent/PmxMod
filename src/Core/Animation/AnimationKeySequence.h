#pragma once

#include <algorithm>

namespace Chrivent {
	// 애니메이션 키 목록의 정렬, 중복 정리와 프레임 검색을 관리한다.
	class AnimationKeySequence {
	public:
		// 키를 프레임순으로 정렬하고 같은 프레임에서는 마지막으로 추가된 키만 남긴다.
		template <typename Keys>
		static void SortAndKeepLastKeyPerFrame(Keys& keys) {
			std::ranges::stable_sort(keys, {}, [](const auto& key) { return key.frame; });
			std::ranges::reverse(keys);
			const auto duplicates = std::ranges::unique(keys, {}, [](const auto& key) { return key.frame; });
			keys.erase(duplicates.begin(), duplicates.end());
			std::ranges::reverse(keys);
		}

		// 지정한 시간보다 큰 첫 번째 키 프레임을 찾는다.
		template <typename Keys>
		static auto FindUpperKey(const Keys& keys, const float t) {
			return std::ranges::upper_bound(keys, t, std::less{}, [](const auto& key) {
				return key.frame;
			});
		}
	};
}
