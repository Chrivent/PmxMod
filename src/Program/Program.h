#pragma once

#include "RendererType.h"
#include "Sound.h"
#include "Manager/CameraManager.h"
#include "Manager/PanelManager.h"
#include "Manager/InputManager.h"
#include "../Viewer/Viewer.h"

namespace Chrivent {
    class Program {
        std::unique_ptr<Viewer> viewer;
        InputManager inputManager;
        CameraManager cameraManager;
        PanelManager panelManager;
        Sound music;
        std::vector<std::unique_ptr<Instance>> instances;
        std::chrono::steady_clock::time_point fpsTime;
        std::chrono::steady_clock::time_point saveTime;
        int fpsFrame = 0;
        RendererType currentRendererType = RendererType::OpenGL;

        // 실행할 렌더러를 선택해 생성한다.
        void CreateViewer(RendererType rendererType);
        // GLFW 윈도우와 렌더러별 리소스를 초기화한다.
        bool InitializeViewer();
        // 확장된 오른쪽 모니터가 있으면 렌더링 창을 해당 작업 영역 중앙에 배치한다.
        void PositionViewerOnRightMonitor() const;
        // 선택한 렌더러로 뷰어 창과 렌더 리소스를 다시 생성한다.
        bool ChangeRenderer(RendererType rendererType);
        // 현재 씬 리소스와 윈도우 리소스를 정리한다.
        void Shutdown();
        // 씬 설정에 맞춰 모델, 애니메이션, 음악, 카메라를 다시 로드한다.
        bool LoadScene(const SceneConfig& sceneConfig);
        // 씬 설정의 모델과 애니메이션을 읽어 렌더 인스턴스를 생성한다.
        bool LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const;
        // 현재 씬의 모델 모션, 카메라 모션, 음악 중 가장 긴 재생 프레임을 계산한다.
        int CalculatePlaybackLastFrame() const;
        // 지정한 프레임으로 이동한 직후 모델 물리 상태를 동기화한다.
        void SyncSeekedPhysics(int frame) const;
        // 현재 렌더 인스턴스들의 GPU 리소스를 해제하고 목록을 비운다.
        void ClearInstances();
        // 창 크기 변경을 렌더러에 반영한다.
        bool UpdateFramebufferSize() const;
        // FPS 표시 시간을 갱신한다.
        void TickFps();
        // 한 프레임의 입력, 시간, 카메라, 렌더링을 처리한다.
        bool RunFrame();
        
    public:
        // 씬 설정을 구성하고 선택한 렌더러로 뷰어를 실행한다.
        bool Run();
    };
}
