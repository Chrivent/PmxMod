#pragma once

#include "Core/Animation/Camera/CameraAnimation.h"
#include "Program/Panel/MotionPanel.h"

#include <filesystem>
#include <span>

namespace Chrivent {
	class Instance;

	// 모션 패널에 적용할 표시 이름과 타임라인 그룹을 묶는다.
	struct MotionTimelineData {
		std::wstring name;
		std::vector<MotionTimelineGroup> groups;
	};

	// 모델과 카메라 애니메이션을 모션 패널용 타임라인 데이터로 변환한다.
	class MotionTimelineBuilder {
		// 같은 프레임의 키를 하나로 합치고 프레임 순서로 정렬한다.
		static void NormalizeKeys(std::vector<MotionTimelineKey>& keys);
		// 그룹 행에 포함된 중복 없는 프레임 목록을 구성한다.
		static std::vector<int> CollectFrames(const std::vector<MotionTimelineRow>& rows);
		// 애니메이션의 부호 없는 프레임을 패널의 int 범위로 변환한다.
		static int ToTimelineFrame(uint32_t frame);
		// 카메라 키를 공통 카메라 타임라인 그룹으로 변환한다.
		static void AppendCameraGroup(std::span<const CameraAnimationKey> cameraKeys,
			std::vector<MotionTimelineGroup>& groups);

	public:
		// 모델의 본, IK와 모프 트랙을 패널용 그룹으로 구성한다.
		static MotionTimelineData BuildModel(const Instance& instance,
			std::span<const CameraAnimationKey> cameraKeys, const std::filesystem::path& fallbackModelPath);
		// 카메라 트랙과 표시 이름을 패널용 데이터로 구성한다.
		static MotionTimelineData BuildCamera(std::span<const CameraAnimationKey> cameraKeys,
			const std::filesystem::path& cameraMotionPath);
	};
}
