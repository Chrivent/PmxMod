#pragma once

#include "Program/RendererType.h"
#include "Program/ResourceDirectories.h"
#include "Program/ShaderEffectController.h"
#include "Program/Sound.h"
#include "Program/Gui/FpsOverlay.h"
#include "Program/Manager/CameraManager.h"
#include "Program/Manager/PanelManager.h"
#include "Program/Manager/InputManager.h"
#include "Program/TaskExecutor.h"
#include "Core/Model/ModelUpdater.h"
#include "Viewer/Viewer/Viewer.h"

#include <chrono>
#include <filesystem>
#include <vector>

namespace Chrivent {
    // 애플리케이션 수명 주기, 씬, 렌더러, 재생과 GUI를 총괄한다.
    class Program {
        // 프레임버퍼 크기 반영의 정상 상태를 렌더 가능과 일시 중단으로 구분한다.
        enum class FramebufferUpdateState {
            Ready,
            Skipped
        };

		// 패널 요청 처리 결과를 계속 실행, 현재 프레임 건너뛰기와 실패로 구분한다.
		enum class FrameRequestState {
			Continue,
			Skip,
			Failed
		};

        // 명령행에서 받은 씬, 렌더러, 벤치마크와 도움말 옵션을 보관한다.
        struct ProgramOptions {
            std::filesystem::path scenePath;
            RendererType rendererType = RendererType::OpenGL;
            std::size_t benchmarkFrames = 0;
            std::size_t warmupFrames = 60;
			bool showHelp = false;
        };

        // 한 프레임의 갱신 및 렌더링 단계별 시간을 보관한다.
        struct FrameTiming {
            double animationMilliseconds = 0.0;
            double initializeCpuMilliseconds = 0.0;
            double animationEvaluateCpuMilliseconds = 0.0;
            double morphCpuMilliseconds = 0.0;
            double beforePhysicsPoseCpuMilliseconds = 0.0;
            double physicsCpuMilliseconds = 0.0;
            double afterPhysicsPoseCpuMilliseconds = 0.0;
            double transformCpuMilliseconds = 0.0;
            double skinningMilliseconds = 0.0;
            double uploadDrawMilliseconds = 0.0;
            double presentMilliseconds = 0.0;
            double totalMilliseconds = 0.0;
        };

		// 모델 애니메이션과 스키닝 단계의 측정 시각을 보관한다.
		struct FrameUpdateTimes {
			std::chrono::steady_clock::time_point animationStart;
			std::chrono::steady_clock::time_point animationEnd;
			std::chrono::steady_clock::time_point skinningEnd;
		};

        std::unique_ptr<Viewer> viewer;
		ResourceDirectories resourceDirectories;
        InputManager inputManager;
        CameraManager cameraManager;
        PanelManager panelManager;
        FpsOverlay fpsOverlay;
        TaskExecutor taskExecutor;
		ShaderEffectController shaderEffectController;
        Sound music;
        std::vector<std::unique_ptr<Instance>> instances;
        std::vector<std::size_t> skinningTaskOffsets;
        std::vector<ModelUpdateTiming> modelUpdateTimings;
        std::chrono::steady_clock::time_point fpsTime;
        std::chrono::steady_clock::time_point saveTime;
        HWND viewerNativeWindow = nullptr;
		WINDOWPLACEMENT viewerWindowedPlacement{ sizeof(WINDOWPLACEMENT) };
		LONG_PTR viewerWindowedStyle = 0;
        int fpsFrame = 0;
        RendererType currentRendererType = RendererType::OpenGL;
        bool benchmarkMode = false;
        bool physicsResetRequested = false;
        bool menuFrameActive = false;
		bool previousSpaceShortcutDown = false;
		bool previousLeftShortcutDown = false;
		bool previousRightShortcutDown = false;
		bool previousF11ShortcutDown = false;
		bool viewerFullscreen = false;

        static constexpr UINT_PTR kViewerWindowSubclassId = 9101;

        // 명령행에서 사용할 수 있는 실행 옵션을 출력한다.
        static void PrintUsage();
        // 감지된 그래픽 API와 GPU 기능을 프로그램 로그에 출력한다.
        static void PrintGraphicsCapabilities(const GraphicsCapabilities& capabilities);
        // 렌더러에서 전파된 구조화된 오류를 프로그램 경계에서 한 번 출력한다.
        static void PrintGraphicsError(const GraphicsError& error);
        // 렌더러 창의 Win32 모달 루프에서도 프레임을 진행한다.
        static LRESULT CALLBACK ViewerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data);
        // 렌더러 이름을 프로그램 렌더러 형식으로 변환한다.
        static bool ParseRenderer(const std::wstring& value, RendererType& rendererType);
        // 양의 정수 명령행 값을 크기 값으로 변환한다.
        static bool ParseCount(const wchar_t* value, std::size_t& count);
        // 명령행 인자를 프로그램 실행 옵션으로 구성한다.
        static bool ParseArguments(int argumentCount, wchar_t* arguments[], ProgramOptions& options);
		// Windows 유니코드 명령행을 읽어 프로그램 실행 옵션으로 구성한다.
		static bool ParseCommandLine(ProgramOptions& options);
        // 렌더러 형식을 성능 결과에 사용할 짧은 이름으로 변환한다.
        static const char* ResolveRendererName(RendererType rendererType);
        // 실행할 렌더러를 선택해 생성한다.
        void CreateViewer(RendererType rendererType);
        // GLFW 윈도우와 렌더러별 리소스를 초기화한다.
        bool InitializeViewer();
        // 렌더러 창 조작 종료를 감지할 Win32 subclass를 설치한다.
		void InstallViewerWindowSubclass(GLFWwindow* window);
        // 렌더러 창에 설치한 Win32 subclass를 해제한다.
        void RemoveViewerWindowSubclass();
		// 렌더러 창을 현재 모니터의 전체화면과 이전 창 상태 사이에서 전환한다.
		void ToggleViewerFullscreen();
        // 현재 프레임에서 물리를 다시 동기화하도록 요청한다.
        void RequestPhysicsReset();
        // 메뉴바 모달 루프 중 렌더링 프레임을 한 번 처리한다.
        bool RunMenuFrame();
        // 확장된 오른쪽 모니터가 있으면 렌더링 창을 해당 작업 영역 중앙에 배치한다.
        static void PositionViewerOnRightMonitor(GLFWwindow* window);
        // 선택한 렌더러로 뷰어 창과 렌더 리소스를 다시 생성한다.
        bool ChangeRenderer(RendererType rendererType);
        // 현재 씬 리소스와 윈도우 리소스를 정리한다.
        void Shutdown();
        // 선택한 모델의 PMX 메타데이터와 런타임 규모를 정보 패널에 표시한다.
        void UpdateModelInformation(std::size_t modelIndex);
        // 씬 설정에 맞춰 모델, 애니메이션, 음악, 카메라를 다시 로드한다.
        bool LoadScene(const SceneConfig& sceneConfig, bool resetPlaybackRange = true);
        // 씬 설정의 모델과 애니메이션을 읽어 렌더 인스턴스를 생성한다.
        bool LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const;
        // 현재 씬의 모델 모션과 카메라 모션 중 가장 긴 프레임을 계산한다.
        int CalculateMotionLastFrame() const;
        // 현재 씬의 모션과 음악 중 가장 긴 자동 재생 프레임을 계산한다.
        int CalculatePlaybackLastFrame() const;
        // 모든 모델의 물리를 초기화하고 지정한 프레임 상태로 다시 동기화한다.
        void ResetPhysics(int frame) const;
        // 선택된 모델의 본, IK, 모프 키프레임을 모션 패널에 표시한다.
        void UpdateMotionPanel(size_t modelIndex);
        // 현재 카메라 모션 키프레임을 모션 패널에 표시한다.
        void UpdateCameraMotionPanel();
        // 현재 렌더 인스턴스들의 GPU 리소스를 해제하고 목록을 비운다.
        void ClearInstances();
        // 창 크기 변경을 렌더러에 반영하고 최소화 상태를 일시 중단으로 구분한다.
        GraphicsError::Result<FramebufferUpdateState> UpdateFramebufferSize() const;
        // FPS 표시 시간을 갱신한다.
        void TickFps();
		// 패널에서 들어온 씬, 렌더러와 셰이더 요청을 처리한다.
		FrameRequestState ProcessPanelRequests();
		// 재생 명령을 카메라 시간, 음악과 물리 상태에 일관되게 반영한다.
		void ApplyPlaybackCommand(PlaybackCommand command);
		// 프레임 탐색과 재생 제어 요청을 카메라와 음악에 반영한다.
		void ProcessPlaybackRequests();
		// 렌더링 창과 패널 창에서 입력한 재생 및 전체화면 단축키를 처리한다.
		void ProcessKeyboardShortcuts();
		// 재생 시간, 물리 초기화와 카메라 상태를 이번 프레임에 맞춰 갱신한다.
		void UpdatePlaybackState();
		// 모델 애니메이션과 스키닝을 병렬 갱신하고 단계별 측정 시각을 반환한다.
		FrameUpdateTimes UpdateInstances(const FrameTiming* timing);
		// 갱신된 인스턴스를 모든 장면 패스에 그려 한 프레임을 완료한다.
		bool RenderScene(FrameTiming* timing, std::chrono::steady_clock::time_point frameStart,
			const FrameUpdateTimes& updateTimes);
        // 한 프레임의 입력, 시간, 카메라, 렌더링을 처리한다.
        bool RunFrame(FrameTiming* timing = nullptr, bool pollGuiWindows = true);
		// 지정한 프레임 수만큼 고정 시간으로 실행하고 구간별 성능 결과를 출력한다.
        int RunBenchmark(std::size_t warmupFrames, std::size_t benchmarkFrames);
		// 해석된 명령행 옵션으로 프로그램 수명 주기를 실행한다.
		int Execute(const ProgramOptions& options);
        
    public:
        Program();
        ~Program();
        
        // 명령행 옵션으로 씬과 렌더러를 구성하고 프로그램을 실행한다.
		static int Run();
    };
}
