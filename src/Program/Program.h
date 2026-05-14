#pragma once

#include "../Viewer/Viewer.h"
#include "Controller.h"
#include "Sound.h"

namespace Chrivent {
    class Program {
        std::unique_ptr<Viewer> viewer;
        Controller controller;
        Sound music;
        std::vector<std::unique_ptr<Instance>> instances;
        std::chrono::steady_clock::time_point fpsTime;
        std::chrono::steady_clock::time_point saveTime;
        int fpsFrame = 0;

        // 실행할 렌더러를 선택해 생성한다.
        void CreateViewer(int engineType);
        // GLFW 윈도우와 렌더러별 리소스를 초기화한다.
        bool InitializeViewer() const;
        // 현재 씬 리소스와 윈도우 리소스를 정리한다.
        void Shutdown();
        // 씬 설정에 맞춰 모델, 애니메이션, 음악, 카메라를 다시 로드한다.
        bool LoadScene(const SceneConfig& sceneConfig);
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
