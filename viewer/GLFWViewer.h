#pragma once

#include "Viewer.h"

class GLFWViewer;

class GLFWShader {
public:
    ~GLFWShader();

    GLuint	program = 0;
    GLint	positionLocation = -1;
    GLint	normalLocation = -1;
    GLint	uvLocation = -1;
    GLint	wvLocation = -1;
    GLint	wvpLocation = -1;
    GLint	ambientLocation = -1;
    GLint	diffuseLocation = -1;
    GLint	specularLocation = -1;
    GLint	specularPowerLocation = -1;
    GLint	alphaLocation = -1;
    GLint	texModeLocation = -1;
    GLint	texLocation = -1;
    GLint	texMulFactorLocation = -1;
    GLint	texAddFactorLocation = -1;
    GLint	sphereTexModeLocation = -1;
    GLint	sphereTexLocation = -1;
    GLint	sphereTexMulFactorLocation = -1;
    GLint	sphereTexAddFactorLocation = -1;
    GLint	toonTexModeLocation = -1;
    GLint	toonTexLocation = -1;
    GLint	toonTexMulFactorLocation = -1;
    GLint	toonTexAddFactorLocation = -1;
    GLint	lightColorLocation = -1;
    GLint	lightDirLocation = -1;

    // 모델 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

class GLFWEdgeShader {
public:
    ~GLFWEdgeShader();

    GLuint	program = 0;
    GLint	positionLocation = -1;
    GLint	normalLocation = -1;
    GLint	wvLocation = -1;
    GLint	wvpLocation = -1;
    GLint	screenSizeLocation = -1;
    GLint	edgeSizeLocation = -1;
    GLint	edgeColorLocation = -1;

    // 엣지 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

class GLFWGroundShadowShader {
public:
    ~GLFWGroundShadowShader();

    GLuint	program = 0;
    GLint	positionLocation = -1;
    GLint	wvpLocation = -1;
    GLint	shadowColorLocation = -1;

    // 지면 그림자 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

struct GLFWTexture {
    GLuint	texture;
    bool	hasAlpha;
};

class GLFWMaterial {
public:
    const Material& mat;
    GLuint  texture = 0;
    bool	textureHasAlpha = false;
    GLuint	spTexture = 0;
    GLuint	toonTexture = 0;

    explicit GLFWMaterial(const Material& mat);
};

class GLFWInstance : public Instance {
public:
    // 모델 데이터를 OpenGL 버퍼, VAO, 재질 리소스로 업로드한다.
    bool Setup(Viewer& viewer) override;
    // OpenGL 버퍼와 VAO 리소스를 해제한다.
    void Clear() override;
    // 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
    void Update() const override;
    // 일반 메시, 엣지, 그림자 패스를 OpenGL로 렌더링한다.
    void Draw() const override;

private:
    GLFWViewer* m_viewer = nullptr;
    GLuint  m_posVbo = 0;
    GLuint	m_norVbo = 0;
    GLuint	m_uvVbo = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType = GL_UNSIGNED_BYTE;
    GLuint	m_vao = 0;
    GLuint	m_edgeVao = 0;
    GLuint	m_gsVao = 0;
    std::vector<GLFWMaterial>   m_materials;
};

class GLFWViewer : public Viewer {
public:
    ~GLFWViewer() override;

    GLuint	    m_dummyColorTex = 0;
    std::unique_ptr<GLFWShader>				        m_shader;
    std::unique_ptr<GLFWEdgeShader>			        m_edgeShader;
    std::unique_ptr<GLFWGroundShadowShader>         m_gsShader;

    // OpenGL 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
    void ConfigureGlfwHints() override;
    // OpenGL 컨텍스트와 셰이더, 기본 텍스처를 초기화한다.
    bool Setup() override;
    // 창 크기에 맞춰 OpenGL 뷰포트와 투영 행렬을 갱신한다.
    bool Resize() override;
    // 컬러/깊이 버퍼를 지우고 프레임 렌더링을 시작한다.
    void BeginFrame() override;
    // GLFW 버퍼를 교체하고 이벤트 처리를 진행한다.
    bool EndFrame() override;
    // OpenGL 모델 인스턴스를 생성한다.
    std::unique_ptr<Instance> CreateInstance() const override;

    // 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
    GLFWTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);

private:
    const int   m_msaaSamples = 4;
    std::map<std::filesystem::path, GLFWTexture>    m_textures;
};
