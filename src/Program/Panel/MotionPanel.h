#pragma once

#include "Panel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Chrivent {
	struct MotionTimelineRow {
		std::wstring name;
		std::vector<uint32_t> keyFrames;
	};

	struct MotionTimelineGroup {
		std::wstring name;
		std::vector<MotionTimelineRow> rows;
		std::vector<uint32_t> keyFrames;
		bool expanded = false;
	};

	class MotionPanel final : public Panel {
		static constexpr int kHeaderHeight = 28;
		static constexpr int kLabelWidth = 150;
		static constexpr int kRowHeight = 22;
		static constexpr int kFrameWidth = 12;

		HWND timelineWindow = nullptr;
		std::wstring modelName;
		std::vector<MotionTimelineGroup> groups;
		uint32_t totalFrame = 1;
		uint32_t currentFrame = 0;
		int firstRow = 0;
		int firstFrame = 0;
		bool seekRequested = false;
		bool seekFinished = false;
		int seekFrame = 0;

		// 모션 타임라인 커스텀 컨트롤의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 현재 크기와 데이터 범위에 맞춰 세로 스크롤바 범위를 갱신한다.
		void UpdateVerticalScrollBar() const;
		// 마지막 프레임과 현재 프레임에 맞춰 가로 스크롤바 범위를 갱신한다.
		void UpdateHorizontalScrollBar() const;
		// 펼쳐진 그룹을 포함해 현재 화면에 표시할 전체 행 수를 반환한다.
		int GetVisibleRowCount() const;
		// 현재 스크롤 위치를 반영해 모션 타임라인을 그린다.
		void Paint(HDC deviceContext) const;
		// 클릭한 표시 행이 그룹이면 펼침 상태를 전환한다.
		void ToggleGroup(int visibleRowIndex);
		// 세로 스크롤 명령을 현재 표시 행에 반영한다.
		void ScrollRows(int scrollCode, int trackPosition);
		// 가로 스크롤 명령을 프레임 이동 요청에 반영한다.
		void ScrollFrames(int scrollCode, int trackPosition);
		// 플레이백 프레임 표시선이 중앙에 도달하면 타임라인을 따라 이동시킨다.
		void FollowCurrentFrame();

	public:
		MotionPanel() = default;

		// 부모 윈도우 아래에 모션 타임라인 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 모션 타임라인 컨트롤 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 모션 타임라인 컨트롤 핸들과 표시 데이터를 정리한다.
		void Destroy() override;
		// 선택 모델의 이름과 트랙별 키프레임을 타임라인에 표시한다.
		void SetTimeline(std::wstring name, std::vector<MotionTimelineGroup> timelineGroups);
		// 가로 스크롤바가 이동할 수 있는 마지막 프레임을 설정한다.
		void SetLastFrame(uint32_t maxFrame);
		// 현재 재생 프레임을 타임라인 표시선에 반영한다.
		void SetCurrentFrame(uint32_t frame);
		// 가로 스크롤바로 요청된 이동 프레임을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished);
	};
}
