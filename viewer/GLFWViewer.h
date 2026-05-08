#pragma once

#include "Viewer.h"

#include <map>

struct GLFWViewer;

struct GLFWShader {
    ~GLFWShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_inNor = -1;
    GLint	m_inUV = -1;
    GLint	m_uWV = -1;
    GLint	m_uWVP = -1;
    GLint	m_uAmbient = -1;
    GLint	m_uDiffuse = -1;
    GLint	m_uSpecular = -1;
    GLint	m_uSpecularPower = -1;
    GLint	m_uAlpha = -1;
    GLint	m_uTexMode = -1;
    GLint	m_uTex = -1;
    GLint	m_uTexMulFactor = -1;
    GLint	m_uTexAddFactor = -1;
    GLint	m_uSphereTexMode = -1;
    GLint	m_uSphereTex = -1;
    GLint	m_uSphereTexMulFactor = -1;
    GLint	m_uSphereTexAddFactor = -1;
    GLint	m_uToonTexMode = -1;
    GLint	m_uToonTex = -1;
    GLint	m_uToonTexMulFactor = -1;
    GLint	m_uToonTexAddFactor = -1;
    GLint	m_uLightColor = -1;
    GLint	m_uLightDir = -1;

    /// 모델 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

struct GLFWEdgeShader {
    ~GLFWEdgeShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_inNor = -1;
    GLint	m_uWV = -1;
    GLint	m_uWVP = -1;
    GLint	m_uScreenSize = -1;
    GLint	m_uEdgeSize = -1;
    GLint	m_uEdgeColor = -1;

    /// 엣지 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

struct GLFWGroundShadowShader {
    ~GLFWGroundShadowShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_uWVP = -1;
    GLint	m_uShadowColor = -1;

    /// 지면 그림자 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GLFWViewer& viewer);
};

struct GLFWTexture {
    GLuint	m_texture;
    bool	m_hasAlpha;
};

struct GLFWMaterial {
    const Material& m_mat;
    GLuint  m_texture = 0;
    bool	m_textureHasAlpha = false;
    GLuint	m_spTexture = 0;
    GLuint	m_toonTexture = 0;

    explicit GLFWMaterial(const Material& mat);
};

struct GLFWInstance : Instance {
    GLFWViewer* m_viewer;
    GLuint  m_posVbo = 0;
    GLuint	m_norVbo = 0;
    GLuint	m_uvVbo = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType;
    GLuint	m_vao = 0;
    GLuint	m_edgeVao = 0;
    GLuint	m_gsVao = 0;
    std::vector<GLFWMaterial>   m_materials;

    /// 모델 데이터를 OpenGL 버퍼, VAO, 재질 리소스로 업로드한다.
    bool Setup(Viewer& viewer) override;
    /// OpenGL 버퍼와 VAO 리소스를 해제한다.
    void Clear() override;
    /// 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
    void Update() const override;
    /// 일반 메시, 엣지, 그림자 패스를 OpenGL로 렌더링한다.
    void Draw() const override;
};

struct GLFWViewer : Viewer {
    ~GLFWViewer() override;

    GLuint	    m_dummyColorTex = 0;
    const int   m_msaaSamples = 4;
    std::unique_ptr<GLFWShader>				        m_shader;
    std::unique_ptr<GLFWEdgeShader>			        m_edgeShader;
    std::unique_ptr<GLFWGroundShadowShader>         m_gsShader;
    std::map<std::filesystem::path, GLFWTexture>    m_textures;

    /// OpenGL 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
    void ConfigureGlfwHints() override;
    /// OpenGL 컨텍스트와 셰이더, 기본 텍스처를 초기화한다.
    bool Setup() override;
    /// 창 크기에 맞춰 OpenGL 뷰포트와 투영 행렬을 갱신한다.
    bool Resize() override;
    /// 컬러/깊이 버퍼를 지우고 프레임 렌더링을 시작한다.
    void BeginFrame() override;
    /// GLFW 버퍼를 교체하고 이벤트 처리를 진행한다.
    bool EndFrame() override;
    /// OpenGL 모델 인스턴스를 생성한다.
    std::unique_ptr<Instance> CreateInstance() const override;

    /// 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
    GLFWTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
};
