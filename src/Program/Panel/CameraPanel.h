#pragma once

#include "Program/Panel/Panel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Chrivent {
	class CameraPanel final : public Panel {
		static constexpr int kCameraMotionRow = 0;
		static constexpr int kShaderRowOffset = 1;

		UINT_PTR addCameraButtonId = 0;
		UINT_PTR deleteCameraButtonId = 0;
		UINT_PTR shaderListId = 0;
		int selectedListIndex = kCameraMotionRow;
		int selectedShaderIndex = -1;
		int pendingSelectedShaderIndex = -1;
		bool pendingShaderEffectEnabled = true;
		bool updatingShaderList = false;
		bool pendingDeleteCameraMotion = false;
		bool pendingCameraMotionSelected = false;
		HWND parentWindow = nullptr;
		HWND addCameraButton = nullptr;
		HWND deleteCameraButton = nullptr;
		HWND shaderList = nullptr;
		std::filesystem::path cameraMotionPath;
		std::filesystem::path pendingCameraMotionPath;
		std::vector<std::wstring> shaderNames;
		std::vector<bool> shaderEnabled;

		// 카메라 VMD 파일을 선택하는 열기 대화상자를 표시한다.
		void ShowOpenCameraMotionDialog();
		// 현재 카메라 모션 파일 이름을 목록 표시용 문자열로 반환한다.
		std::wstring ResolveCameraMotionText() const;
		// 현재 셰이더 이름 목록을 리스트 컨트롤에 반영한다.
		void RefreshShaderList();
		// 카메라 행과 셰이더 행의 선택/체크 상태를 리스트뷰에 반영한다.
		void ApplyListState();
		// 셰이더 목록에서 선택하거나 체크한 항목을 대기 요청으로 기록한다.
		void QueueShaderSelection(int shaderIndex, bool enabled);
		// 카메라 모션 행 선택 요청을 기록한다.
		void QueueCameraMotionSelection();

	public:
		CameraPanel() = default;

		// 카메라 모션 버튼과 셰이더 목록의 컨트롤 ID를 적용한다.
		void ApplyControlIds(const UINT_PTR addId, const UINT_PTR deleteId, const UINT_PTR listId) {
			addCameraButtonId = addId;
			deleteCameraButtonId = deleteId;
			shaderListId = listId;
		}

		// 부모 윈도우 아래에 카메라 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 카메라 모션과 셰이더 목록 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 카메라 패널의 컨트롤 표시 상태를 갱신한다.
		void UpdateVisibility(bool visible) const;
		// 현재 언어에 맞춰 버튼 문구와 카메라 모션 표시를 갱신한다.
		void UpdateLanguage() override;
		// 카메라 모션 버튼과 셰이더 목록의 명령을 처리한다.
		bool HandleCommand(UINT_PTR commandId, int notificationCode) override;
		// 셰이더 리스트뷰의 선택과 체크박스 변경 알림을 처리한다.
		bool HandleNotify(const NMHDR& notifyHeader, LRESULT& result) override;
		// 카메라 패널 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 선택된 셰이더 인덱스를 반환하고 대기 중인 요청을 초기화한다.
		bool ConsumeSelectedShaderIndex(size_t& shaderIndex, bool& enabled);
		// 카메라 모션 행 선택 요청을 반환하고 대기 중인 요청을 초기화한다.
		bool ConsumeCameraMotionSelected();
		// 추가한 카메라 모션 경로를 반환하고 대기 중인 요청을 초기화한다.
		bool ConsumeCameraMotionPath(std::filesystem::path& motionPath);
		// 카메라 모션 삭제 요청을 반환하고 내부 상태를 초기화한다.
		bool ConsumeDeleteCameraMotion();
		// 현재 씬의 카메라 모션 경로를 패널에 반영한다.
		void UpdateCameraMotionPath(const std::filesystem::path& motionPath);
		// 검색된 셰이더 이름과 현재 선택을 패널에 반영한다.
		void UpdateShaderNames(const std::vector<std::wstring>& names, size_t selectedIndex, const std::vector<bool>& enabledStates);
	};
}
