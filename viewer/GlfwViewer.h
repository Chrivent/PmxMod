#pragma once

#include "Viewer.h"

#include <string>

class GlfwViewer;

class GlfwShaderHelper {
public:
    // GLSL 셰이더 소스를 지정한 타입으로 컴파일한다.
    static GLuint CompileShader(GLenum shaderType, const std::string& code);
    // 단일 GLSL 파일에서 버텍스/프래그먼트 분기를 위한 define 줄을 삽입한다.
    static std::string InjectDefine(const std::string& src, const char* defineLine);
    // GLSL 파일을 읽어 버텍스/프래그먼트 셰이더 프로그램을 생성한다.
    static GLuint CreateShader(const std::filesystem::path& file);
};

class GlfwShader {
public:
    ~GlfwShader();

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
    GLint	cartoonTexModeLocation = -1;
    GLint	cartoonTexLocation = -1;
    GLint	cartoonTexMulFactorLocation = -1;
    GLint	cartoonTexAddFactorLocation = -1;
    GLint	lightColorLocation = -1;
    GLint	lightDirLocation = -1;

    // 모델 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GlfwViewer& viewer);

private:
    // uniform 이름 목록을 조회해 대응하는 위치 포인터들에 저장한다.
    static void LoadUniformLocations(GLuint program, const char* const* names, GLint* const* outs, int count);
};

class GlfwEdgeShader {
public:
    ~GlfwEdgeShader();

    GLuint	program = 0;
    GLint	positionLocation = -1;
    GLint	normalLocation = -1;
    GLint	wvLocation = -1;
    GLint	wvpLocation = -1;
    GLint	screenSizeLocation = -1;
    GLint	edgeSizeLocation = -1;
    GLint	edgeColorLocation = -1;

    // 엣지 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GlfwViewer& viewer);
};

class GlfwGroundShadowShader {
public:
    ~GlfwGroundShadowShader();

    GLuint	program = 0;
    GLint	positionLocation = -1;
    GLint	wvpLocation = -1;
    GLint	shadowColorLocation = -1;

    // 지면 그림자 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
    bool Setup(const GlfwViewer& viewer);
};

struct GlfwTexture {
    GLuint	texture;
    bool	hasAlpha;
};

class GlfwMaterial {
public:
    const Material& mat;
    GLuint  texture = 0;
    bool	textureHasAlpha = false;
    GLuint	sphereTexture = 0;
    GLuint	cartoonTexture = 0;

    explicit GlfwMaterial(const Material& sourceMat);
};

class GlfwInstance : public Instance {
public:
    ~GlfwInstance() override;

    // 모델 데이터를 OpenGL 버퍼, VAO, 재질 리소스로 업로드한다.
    bool Setup(Viewer& baseViewer) override;
    // OpenGL 버퍼와 VAO 리소스를 해제한다.
    void Clear() override;
    // 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
    void Update() const override;
    
protected:
    // 일반 메시 패스를 OpenGL로 렌더링한다.
    void DrawModel() const override;
    // 엣지 패스를 OpenGL로 렌더링한다.
    void DrawEdge() const override;
    // 지면 그림자 패스를 OpenGL로 렌더링한다.
    void DrawGroundShadow() const override;
    // OpenGL 버퍼를 생성하고 초기 데이터를 업로드한다.
    static GLuint CreateBuffer(GLenum target, size_t size, const void* data, GLenum usage);
    // 지정한 버퍼와 attribute 정보를 묶은 VAO를 생성한다.
    static GLuint CreateVao(const GLuint* buffers, const GLint* locs, const GLint* sizes, const GLenum* types,
        int attribCount, GLuint ibo);

    GlfwViewer* viewer = nullptr;
    GLuint  posVbo = 0;
    GLuint	norVbo = 0;
    GLuint	uvVbo = 0;
    GLuint	ibo = 0;
    GLenum	indexType = GL_UNSIGNED_BYTE;
    GLuint	vao = 0;
    GLuint	edgeVao = 0;
    GLuint	gsVao = 0;
    std::vector<GlfwMaterial>   materials;
};

class GlfwViewer : public Viewer {
public:
    ~GlfwViewer() override;

    GLuint	    dummyColorTex = 0;
    std::unique_ptr<GlfwShader>				        shader;
    std::unique_ptr<GlfwEdgeShader>			        edgeShader;
    std::unique_ptr<GlfwGroundShadowShader>         gsShader;

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
    GlfwTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);

private:
    // GLAD가 사용할 OpenGL 함수 포인터를 GLFW에서 조회한다.
    static void* LoadGlProc(const char* name);

    const int   msaaSamples = 4;
    std::map<std::filesystem::path, GlfwTexture>    textures;
};
