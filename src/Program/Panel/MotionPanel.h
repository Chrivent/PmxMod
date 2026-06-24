#pragma once

#include "Program/Panel/InterpolationCurvePanel.h"
#include "Program/Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	struct AudioWaveform;

	enum class MotionTimelineMode {
		Model,
		Camera
	};

	struct MotionTimelineKey {
		int frame = 0;
		std::vector<BezierControlPoints> curves;
		std::vector<float> values;
		bool selected = false;
	};

	struct MotionTimelineRow {
		std::wstring name;
		std::vector<std::wstring> curveNames;
		std::vector<MotionTimelineKey> keys;
		bool expandable = false;
		bool expanded = false;
	};

	struct MotionTimelineGroup {
		std::wstring name;
		std::vector<MotionTimelineRow> rows;
		std::vector<int> keyFrames;
		MotionTimelineMode mode = MotionTimelineMode::Model;
		bool grouped = true;
		bool expanded = false;
	};

	class MotionPanel final : public Panel {
		static constexpr UINT_PTR kFrameEditId = 4001;
		static constexpr UINT_PTR kModeButtonId = 4002;
		static constexpr int kMaxEditableFrame = 65535;
		static constexpr int kHeaderHeight = 28;
		static constexpr int kLabelWidth = 150;
		static constexpr int kRowHeight = 22;
		static constexpr int kCurveRowHeight = 66;
		static constexpr int kCurveGraphHeight = kCurveRowHeight * 4;
		static constexpr int kFrameWidth = 12;
		static constexpr int kWaveformHeight = 88;

		HWND timelineWindow = nullptr;
		HWND frameEdit = nullptr;
		HWND modeButton = nullptr;
		std::wstring modelName;
		std::vector<MotionTimelineGroup> groups;
		const AudioWaveform* waveform = nullptr;
		MotionTimelineMode mode = MotionTimelineMode::Camera;
		POINT selectionStart{};
		POINT selectionEnd{};
		int totalFrame = 0;
		int currentFrame = 0;
		int firstRow = 0;
		int firstFrame = 0;
		bool seekRequested = false;
		bool seekFinished = false;
		bool interpolationSelectionDirty = false;
		bool updatingFrameEdit = false;
		bool selectingKeys = false;
		bool playing = false;
		bool scrollingFrameThumb = false;
		int seekFrame = 0;

		// 모션 타임라인 커스텀 컨트롤의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 프레임 입력 칸의 Enter와 포커스 해제를 처리한다.
		static LRESULT CALLBACK EditWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data);
		// 상단 입력값을 독립 프레임 이동 요청으로 반영한다.
		void ApplyInputFrame();
		// 상단 입력 칸이 포커스를 잃으면 스크롤 범위 안으로 복귀한다.
		void ClampInputFrameToScrollRange();
		// 현재 프레임을 상단 입력 칸에 표시한다.
		void UpdateFrameEditText(bool force = false);
		// 현재 크기와 데이터 범위에 맞춰 세로 스크롤바 범위를 갱신한다.
		void UpdateVerticalScrollBar() const;
		// 마지막 프레임과 현재 프레임에 맞춰 가로 스크롤바 범위를 갱신한다.
		void UpdateHorizontalScrollBar() const;
		// 고정 파형 트랙을 제외한 타임라인 행 영역의 아래쪽 좌표를 반환한다.
		int ResolveTimelineBottom() const;
		// 펼쳐진 곡선 행을 포함해 현재 모드에서 표시할 전체 행 수를 계산한다.
		int CalculateVisibleRowCount() const;
		// 현재 가로 프레임 범위에 맞춰 하단 단일 채널 오디오 파형을 그린다.
		void DrawWaveform(HDC deviceContext, int top, int right, int bottom) const;
		// 현재 스크롤 위치를 반영해 모션 타임라인을 그린다.
		void Paint(HDC deviceContext) const;
		// 클릭한 표시 행이 그룹이면 펼침 상태를 전환한다.
		void ToggleGroup(int visibleRowIndex);
		// 모든 채널의 키 값과 보간 데이터를 하나의 타임라인 곡선 영역에 겹쳐 그린다.
		void DrawValueCurves(HDC deviceContext, const MotionTimelineRow& row, int top, int bottom, int right) const;
		// 클릭한 다이아몬드를 단일 또는 다중 선택 상태로 반영한다.
		bool SelectKey(int visibleRowIndex, int x, int y, bool additive);
		// 드래그 사각형 안의 다이아몬드를 선택 상태로 반영한다.
		void SelectKeysInRectangle(bool additive);
		// 모든 실제 키의 선택 상태를 해제한다.
		void ClearKeySelection();
		// 현재 모드에서 표시할 그룹인지 확인한다.
		bool IsGroupVisible(const MotionTimelineGroup& group) const { return group.mode == mode; }
		// 그룹의 해당 프레임에 선택된 실제 키가 있는지 확인한다.
		static bool IsGroupFrameSelected(const MotionTimelineGroup& group, int frame);
		// 현재 선택된 키와 채널별 보간 곡선을 보간 패널용 데이터로 구성한다.
		InterpolationSelection BuildInterpolationSelection() const;
		// 모델과 카메라 타임라인 표시 모드를 전환한다.
		void ToggleMode();
		// 현재 모드에 맞춰 전환 버튼 문구를 갱신한다.
		void UpdateModeButtonText() const;
		// 세로 스크롤 명령을 현재 표시 행에 반영한다.
		void ScrollRows(int scrollCode, int trackPosition);
		// 가로 스크롤 명령을 프레임 이동 요청에 반영한다.
		void ScrollFrames(int scrollCode, int trackPosition);
		// 플레이백 프레임 표시선이 중앙에 도달하면 타임라인을 따라 이동시킨다.
		void FollowCurrentFrame();

	public:
		MotionPanel() = default;

		MotionTimelineMode GetMode() const { return mode; }

		// 재생 상태에 따라 독립 프레임 입력을 활성화한다.
		void ApplyPlaybackState(bool isPlaying);
		// 오디오 파형 데이터를 연결하고 타임라인을 다시 그린다.
		void AttachWaveform(const AudioWaveform& audioWaveform);
		// 모델 또는 카메라 타임라인 모드로 전환한다.
		void ChangeMode(MotionTimelineMode timelineMode);
		// 부모 윈도우 아래에 모션 타임라인 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 모션 타임라인 컨트롤 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 현재 언어에 맞춰 모션 패널의 고정 문구를 다시 그린다.
		void UpdateLanguage() override;
		// 상단 프레임 입력 칸의 변경과 포커스 해제를 처리한다.
		bool HandleCommand(UINT_PTR commandId, int notificationCode) override;
		// 모션 타임라인 컨트롤 핸들과 표시 데이터를 정리한다.
		void Destroy() override;
		// 표시할 이름과 타임라인 그룹을 교체한다.
		void ApplyTimeline(std::wstring name, std::vector<MotionTimelineGroup> timelineGroups);
		// 가로 스크롤의 마지막 프레임을 갱신한다.
		void UpdateLastFrame(int maxFrame);
		// 현재 프레임과 가이드바, 입력 표시를 갱신한다.
		void UpdateCurrentFrame(int frame);
		// 가로 스크롤바로 요청된 이동 프레임을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished);
		// 변경된 키 선택과 보간 곡선 정보를 반환한다.
		bool ConsumeInterpolationSelection(InterpolationSelection& selection);
	};
}
