#pragma once

#include <memory>
#include <filesystem>
#include <glm/glm.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Instance.h"

namespace Chrivent {
    struct Material;
    
    struct ViewerMaterial {
        const Material& mat;

        virtual ~ViewerMaterial() = default;

        explicit ViewerMaterial(const Material& sourceMat) : mat(sourceMat) {}
    };
    
    struct ViewerInfo {
        std::filesystem::path	shaderDir;
        std::filesystem::path	pmxDir;
        glm::mat4	            viewMat;
        glm::mat4	            projMat;
        int			            screenWidth = 0;
        int			            screenHeight = 0;
        glm::vec3	            lightColor = glm::vec3(1, 1, 1);
        glm::vec3	            lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
        float	                elapsed = 0.0f;
        float	                animTime = 0.0f;
        bool	                skipPhysics = false;
        GLFWwindow*             window = nullptr;

        virtual ~ViewerInfo() = default;
    };

    class Viewer {
    protected:
        std::unique_ptr<ViewerInfo> info;
        std::filesystem::path	resourceDir;
        float clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
        HWND fpsOverlay = nullptr;
        HFONT fpsFont = nullptr;

        // FPS 오버레이 윈도우의 그리기 메시지를 처리한다.
        static LRESULT CALLBACK FpsOverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    public:
        Viewer();
        virtual ~Viewer();

        ViewerInfo& GetInfo() { return *info; }
        const ViewerInfo& GetInfo() const { return *info; }

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
        static unsigned char* LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp);
        // 실행 파일 기준 리소스, 셰이더, PMX 디렉터리를 초기화한다.
        void InitDirs(const std::filesystem::path& shaderSubDir);
        // 렌더링 창 좌측 상단에 FPS 오버레이를 생성한다.
        void CreateFpsOverlay();
        // FPS 오버레이의 표시 여부를 설정한다.
        void SetFpsVisible(bool visible) const;
        // FPS 오버레이에 현재 측정값을 표시한다.
        void UpdateFps(double fps) const;

    };
}
