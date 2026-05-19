#pragma once

#include "CameraAnimation.h"
#include "../../Reader/VmdParser.h"

namespace Chrivent {
	class CameraAnimationBuilder {
		CameraAnimation& animation;
		
		// VMD 카메라 키를 런타임 카메라 애니메이션 키로 변환한다.
		static CameraAnimationKey CreateCameraKey(const auto& camera);

	public:
		explicit CameraAnimationBuilder(CameraAnimation& animation) : animation(animation) {}

		// VMD 카메라 데이터를 카메라 애니메이션 키에 추가한다.
		bool Add(const VmdParser& vmd) const;
	};
}
