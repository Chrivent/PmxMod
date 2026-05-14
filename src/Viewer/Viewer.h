#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../Animation/Animation.h"
#include "../Program/Controller.h"

namespace Chrivent {
    struct Material;
    class Viewer;
    class Animation;
    class Model;

    class Instance {
    protected:
        // 일반 메시 패스를 그린다.
        virtual void DrawModel() const = 0;
        // 엣지 패스를 그린다.
        virtual void DrawEdge() const = 0;
        // 지면 그림자 패스를 그린다.
        virtual void DrawGroundShadow() const = 0;
    
    public:
        virtual ~Instance();

        std::shared_ptr<Model>	    model;
        std::unique_ptr<Animation>	anim;
        float scale = 1.0f;

        // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
        virtual bool Setup(Viewer& baseViewer) = 0;
        // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
        virtual void Update() const = 0;
        // 현재 인스턴스를 패스 순서대로 화면에 그린다.
        void Draw() const;
        // 렌더러별 GPU 리소스를 해제한다.
        virtual void Clear() {}
        // 뷰어 시간과 애니메이션 설정을 기준으로 모델 애니메이션을 갱신한다.
        void UpdateAnimation(const Viewer& viewer) const;
    };

    class Viewer {
    protected:
        // FPS 표시 시간을 갱신한다.
        static void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame);

        std::filesystem::path	resourceDir;
        Controller controller;
        float clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
    
    public:
        virtual ~Viewer();

        // 씬 설정을 로드하고 메인 렌더 루프를 실행한다.
        bool Run(const SceneConfig& cfg);

        // 렌더러별 GLFW 윈도우 힌트를 설정한다.
        virtual void ConfigureGlfwHints() = 0;
        // 렌더러와 공통 뷰어 리소스를 초기화한다.
        virtual bool Setup() = 0;
        // 창 크기에 맞춰 렌더 타깃과 투영 행렬을 갱신한다.
        virtual bool Resize() = 0;
        // 한 프레임의 렌더링 시작 상태를 준비한다.
        virtual void BeginFrame() = 0;
        // 한 프레임의 렌더링을 종료하고 표시 결과를 제출한다.
        virtual bool EndFrame() = 0;
        // 현재 렌더러에 맞는 모델 인스턴스를 생성한다.
        virtual std::unique_ptr<Instance> CreateInstance() const = 0;

        // 이미지 파일을 RGBA 픽셀 데이터로 로드한다.
        static unsigned char* LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp, bool flipY = false);
        // 씬 설정의 모델과 애니메이션을 읽어 렌더 인스턴스를 생성한다.
        bool LoadInstances(const SceneConfig& cfg, std::vector<std::unique_ptr<Instance>>& instances);
        // 실행 파일 기준 리소스, 셰이더, PMX 디렉터리를 초기화한다.
        void InitDirs(const std::filesystem::path& shaderSubDir);

        std::filesystem::path	shaderDir;
        std::filesystem::path	pmxDir;
        glm::mat4	viewMat;
        glm::mat4	projMat;
        int			screenWidth = 0;
        int			screenHeight = 0;
        glm::vec3	lightColor = glm::vec3(1, 1, 1);
        glm::vec3	lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
        float	elapsed = 0.0f;
        float	animTime = 0.0f;
        GLFWwindow* window = nullptr;
    };
}
