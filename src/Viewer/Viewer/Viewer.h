#pragma once

#include <memory>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Viewer/Device/GraphicsCapabilities.h"
#include "Viewer/Instance/Instance.h"
#include "Viewer/Shader/ShaderPackage.h"

namespace Chrivent {
    struct Material;
	class PostProcess;

    // 시간 기반 후처리 패스가 공유하는 현재 프레임과 카메라 입력을 보관한다.
    struct PostProcessFrameData {
        float deltaTime = 0.0f;
        float nearPlane = 1.0f;
        float farPlane = 10000.0f;
        float verticalFovRadians = 0.5235988f;
        float viewportWidth = 0.0f;
        float viewportHeight = 0.0f;
        float inverseViewportWidth = 0.0f;
        float inverseViewportHeight = 0.0f;
        float historyReset = 1.0f;
        float padding0 = 0.0f;
        float padding1 = 0.0f;
        float padding2 = 0.0f;
        glm::vec4 cameraWorldPosition{};
        glm::vec4 previousCameraWorldPosition{};
        glm::vec4 cameraWorldDirection{};
        glm::vec4 previousCameraWorldDirection{};
        glm::vec4 cameraWorldRight{};
        glm::vec4 cameraWorldUp{};
    };
    
    // 렌더링 API별 material이 공유하는 원본 PMX 재질 참조를 보관한다.
    struct ViewerMaterial {
        const Material& mat;

        explicit ViewerMaterial(const Material& sourceMat) : mat(sourceMat) {}

        virtual ~ViewerMaterial() = default;
    };
    
	// 렌더링 API 구현이 따라야 할 장면 렌더링과 후처리 공통 계약을 정의한다.
	class Viewer {
		std::filesystem::path resourceDir;
		std::filesystem::path internalShaderDir;
		HWND fpsOverlay = nullptr;
		HFONT fpsFont = nullptr;

		// FPS 오버레이 윈도우의 그리기 메시지를 처리한다.
		static LRESULT CALLBACK FpsOverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 실행 파일을 기준으로 리소스 디렉터리를 초기화한다.
		void InitializeDirectories();
		// 엔진 장면 입력 HLSL의 파일명과 진입점을 공통 역할 계약으로 구성한다.
		bool LoadSceneInputShaderContract();
		// 기본 패키지에서 모델, 엣지, 지면 그림자 패스 계약을 읽는다.
		bool LoadBuiltInShaderContract();
		// 렌더링 창 클라이언트 좌측 상단에 FPS 오버레이를 배치한다.
		void PositionFpsOverlay() const;

	protected:
		BuiltInShaderPasses builtInShaderPasses;
		SceneInputShaderPasses sceneInputShaderPasses;
		float clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };

		// API 구현이 소유한 포스트 프로세서를 반환한다.
		virtual PostProcess& ResolvePostProcess() = 0;
		// API 구현이 소유한 읽기 전용 포스트 프로세서를 반환한다.
		virtual const PostProcess& ResolvePostProcess() const = 0;
		// 리소스 디렉터리와 기본 셰이더 계약을 초기화한다.
		bool InitializeShaderResources();
		// 후처리 로드 성공 시 공통 프레임 히스토리를 초기화한다.
		bool FinishPostProcessLoad(bool loaded);
		// 다음 프레임에서 시간 기반 후처리 입력을 현재 상태로 초기화한다.
		void ResetPostProcessFrameHistory();

	public:
		std::filesystem::path pmxDir;
		glm::mat4 viewMat;
		glm::mat4 projMat;
		glm::mat4 previousViewMat{1.0f};
		glm::mat4 previousProjMat{1.0f};
		PostProcessFrameData postProcessFrameData;
		int screenWidth = 0;
		int screenHeight = 0;
		glm::vec3 lightColor = glm::vec3(1, 1, 1);
		glm::vec3 lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
		float elapsed = 0.0f;
		float renderDeltaTime = 0.0f;
		float animTime = 0.0f;
		bool skipPhysics = false;
		bool modelEffectEnabled = true;
		bool edgeEffectEnabled = true;
		bool groundShadowEffectEnabled = true;
		bool postProcessHistoryResetPending = true;
		GLFWwindow* window = nullptr;
		GraphicsCapabilities capabilities;

		Viewer() = default;
		virtual ~Viewer();

		// 렌더러별 GLFW 윈도우 힌트를 설정한다.
		virtual void ConfigureWindowHints() = 0;
		// 렌더러와 공통 뷰어 리소스를 초기화한다.
		virtual bool Setup() = 0;
		// 창 크기에 맞춰 렌더 타깃과 투영 행렬을 갱신한다.
		virtual bool Resize() = 0;
		// 한 프레임의 렌더링 시작 상태를 준비한다.
		virtual void BeginFrame() = 0;
		// 한 프레임의 렌더링을 종료하고 표시 결과를 제출한다.
		virtual bool EndFrame() = 0;
		// 포스트 프로세스용 depth-only 패스를 시작한다.
		virtual bool BeginPostProcessDepthPass() = 0;
		// 포스트 프로세스용 depth-only 패스를 종료한다.
		virtual void EndPostProcessDepthPass() = 0;
		// 렌더러가 제출한 GPU 작업이 모두 끝날 때까지 기다린다.
		virtual void WaitIdle() = 0;
		// 체크된 포스트 프로세스 효과의 선언형 리소스와 패스 그래프를 렌더러에 준비한다.
		// HLSL 입력은 FrameData=b0, 패스별 JSON reads=t0~t7, LinearClamp=s0 규격을 사용한다.
		virtual bool LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) = 0;
		// 현재 렌더러에 맞는 모델 인스턴스를 생성한다.
		virtual std::unique_ptr<Instance> CreateInstance() const = 0;
		// 실행 파일 리소스 아래의 셰이더 패키지 디렉터리를 반환한다.
		std::filesystem::path ResolveShaderPackagesDirectory() const { return resourceDir / "shaders"; }
		// 카메라 점프나 탐색 뒤 다음 프레임의 temporal history를 초기화한다.
		void ResetPostProcessHistory();
		// 활성 후처리 효과가 장면 속도 입력을 요구하는지 반환한다.
		bool RequiresPostProcessVelocity() const;
		// 표시가 끝난 카메라 행렬을 다음 프레임의 이전 상태로 확정한다.
		void CommitPostProcessFrameHistory();
		// 렌더링 창 좌측 상단에 FPS 오버레이를 생성한다.
		void CreateFpsOverlay();
		// FPS 오버레이에 현재 측정값을 표시한다.
		void UpdateFps(double fps) const;
		// FPS 오버레이의 표시 상태와 위치를 갱신한다.
		void UpdateFpsVisibility(bool visible) const;
	};
}
