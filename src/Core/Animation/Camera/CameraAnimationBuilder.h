#pragma once

#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Parser/VmdParser.h"

namespace Chrivent {
	// VMD 카메라 키를 런타임 카메라 애니메이션으로 변환한다.
	class CameraAnimationBuilder {
		// VMD 카메라 키를 런타임 카메라 애니메이션 키로 변환한다.
		static CameraAnimationKey CreateCameraKey(const VmdParser::VmdCamera& camera);

	public:
		// VMD 카메라 데이터를 런타임 카메라 애니메이션 키 목록으로 변환한다.
		static std::vector<CameraAnimationKey> Build(const VmdParser::VmdData& vmdData);
	};
}
