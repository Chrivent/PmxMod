#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../src/animation/ModelAnimation.h"
#include "../src/animation/CameraAnimation.h"
#include "../src/Sound.h"

struct SceneConfig;
struct Material;
class Viewer;
class ModelAnimation;
class Model;

struct ModelConfig {
    std::filesystem::path				modelPath;
    std::vector<std::filesystem::path>	animPaths;
    float								scale = 1.0f;
};

struct SceneConfig {
    std::vector<ModelConfig>    modelConfigs;
    std::filesystem::path	    cameraAnim;
    std::filesystem::path	    musicPath;
};

class Instance {
public:
    virtual ~Instance();

    std::shared_ptr<Model>	    model;
    std::unique_ptr<ModelAnimation>	anim;
    float scale = 1.0f;

    // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
    virtual bool Setup(Viewer& baseViewer) = 0;
    // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
    virtual void Update() const = 0;
    // 현재 인스턴스를 화면에 그린다.
    virtual void Draw() const = 0;
    // 렌더러별 GPU 리소스를 해제한다.
    virtual void Clear() {}

    // 뷰어 시간과 애니메이션 설정을 기준으로 모델 애니메이션을 갱신한다.
    void UpdateAnimation(const Viewer& viewer) const;

};

class Viewer {
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
    // 씬 설정의 카메라 VMD를 로드한다.
    void LoadCameraAnim(const SceneConfig& cfg);
    // 음악 재생 위치와 프레임 시간을 기준으로 애니메이션 시간을 진행한다.
    void StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime);
    // 키보드와 마우스 입력을 처리해 재생/카메라 상태를 변경한다.
    void HandleInput(Sound& music);
    // 모션 카메라 또는 자유 카메라 상태로 뷰 행렬을 갱신한다.
    void UpdateCamera();
    // 현재 뷰 행렬을 자유 카메라 위치/회전 상태로 동기화한다.
    void SyncFreeCameraToCurrentView();
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

protected:
    std::filesystem::path	resourceDir;
    bool    paused = false;
    bool    prevSpaceDown = false;
    bool    useMotionCamera = true;
    bool    hasFreeCameraState = false;
    bool    prevRDown = false;
    bool    prevRightMouseDown = false;
    double  prevCursorX = 0.0;
    double  prevCursorY = 0.0;
    glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
    float   freeCamYaw = glm::radians(-90.0f);
    float   freeCamPitch = 0.0f;
    std::unique_ptr<CameraAnimation>	cameraAnim;
    float clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
};
