#pragma once

#include "RendererType.h"
#include "Sound.h"
#include "Manager/CameraManager.h"
#include "Manager/PanelManager.h"
#include "Manager/InputManager.h"
#include "TaskExecutor.h"
#include "../Viewer/Viewer.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace Chrivent {
    class Program {
        struct ProgramOptions {
            std::filesystem::path scenePath;
            RendererType rendererType = RendererType::OpenGL;
            std::size_t benchmarkFrames = 0;
            std::size_t warmupFrames = 60;
        };

        struct FrameTiming {
            double animationMilliseconds = 0.0;
            double skinningMilliseconds = 0.0;
            double uploadDrawMilliseconds = 0.0;
            double presentMilliseconds = 0.0;
            double totalMilliseconds = 0.0;
        };

        std::unique_ptr<Viewer> viewer;
        InputManager inputManager;
        CameraManager cameraManager;
        PanelManager panelManager;
        TaskExecutor taskExecutor;
        Sound music;
        std::vector<std::unique_ptr<Instance>> instances;
        std::vector<std::size_t> skinningTaskOffsets;
        std::chrono::steady_clock::time_point fpsTime;
        std::chrono::steady_clock::time_point saveTime;
        int fpsFrame = 0;
        RendererType currentRendererType = RendererType::OpenGL;
        bool benchmarkMode = false;

        // 명령행에서 사용할 수 있는 실행 옵션을 출력한다.
        static void PrintUsage();
        // 렌더러 이름을 프로그램 렌더러 형식으로 변환한다.
        static bool ParseRenderer(std::wstring_view value, RendererType& rendererType);
        // 양의 정수 명령행 값을 크기 값으로 변환한다.
        static bool ParseCount(const wchar_t* value, std::size_t& count);
        // 명령행 인자를 프로그램 실행 옵션으로 구성한다.
        static bool ParseArguments(
            int argumentCount,
            wchar_t* arguments[],
            ProgramOptions& options);
        // 렌더러 형식을 성능 결과에 사용할 짧은 이름으로 변환한다.
        static const char* GetRendererName(RendererType rendererType);
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
        bool LoadScene(const SceneConfig& sceneConfig, bool resetPlaybackRange = true);
        // 씬 설정의 모델과 애니메이션을 읽어 렌더 인스턴스를 생성한다.
        bool LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const;
        // 현재 씬의 모델 모션과 카메라 모션 중 가장 긴 프레임을 계산한다.
        int CalculateMotionLastFrame() const;
        // 현재 씬의 모션과 음악 중 가장 긴 자동 재생 프레임을 계산한다.
        int CalculatePlaybackLastFrame() const;
        // 지정한 프레임으로 이동한 직후 모델 물리 상태를 동기화한다.
        void SyncSeekedPhysics(int frame) const;
        // 선택된 모델의 본, IK, 모프 키프레임을 모션 패널에 표시한다.
        void UpdateMotionPanel(size_t modelIndex);
        // 현재 렌더 인스턴스들의 GPU 리소스를 해제하고 목록을 비운다.
        void ClearInstances();
        // 창 크기 변경을 렌더러에 반영한다.
        bool UpdateFramebufferSize() const;
        // FPS 표시 시간을 갱신한다.
        void TickFps();
        // 한 프레임의 입력, 시간, 카메라, 렌더링을 처리한다.
        bool RunFrame(FrameTiming* timing = nullptr);
        // 지정한 프레임 수만큼 고정 시간으로 실행하고 구간별 성능 결과를 출력한다.
        int RunBenchmark(std::size_t warmupFrames, std::size_t benchmarkFrames);
        
    public:
        // 명령행 옵션으로 씬과 렌더러를 구성하고 프로그램을 실행한다.
        int Run(int argumentCount, wchar_t* arguments[]);
    };
}
