#pragma once

#include "../Viewer.h"
#include "GlfwTextureCache.h"

namespace Chrivent {
    class GlfwViewer;

    struct GlfwShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;
        GLint   wvpLocation = -1;

        virtual ~GlfwShader();
    };

    struct GlfwModelShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;
        GLint   wvLocation = -1;
        GLint   ambientLocation = -1;
        GLint   diffuseLocation = -1;
        GLint   specularLocation = -1;
        GLint   specularPowerLocation = -1;
        GLint   alphaLocation = -1;
        GLint   texModeLocation = -1;
        GLint   texLocation = -1;
        GLint   texMulFactorLocation = -1;
        GLint   texAddFactorLocation = -1;
        GLint   sphereTexModeLocation = -1;
        GLint   sphereTexLocation = -1;
        GLint   sphereTexMulFactorLocation = -1;
        GLint   sphereTexAddFactorLocation = -1;
        GLint   toonTexModeLocation = -1;
        GLint   toonTexLocation = -1;
        GLint   toonTexMulFactorLocation = -1;
        GLint   toonTexAddFactorLocation = -1;
        GLint   lightColorLocation = -1;
        GLint   lightDirLocation = -1;

        // 紐⑤뜽 ?뚮뜑留??곗씠???꾨줈洹몃옩??而댄뙆?쇳븯怨?uniform ?꾩튂瑜?議고쉶?쒕떎.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwEdgeShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   wvLocation = -1;
        GLint   screenSizeLocation = -1;
        GLint   edgeSizeLocation = -1;
        GLint   edgeColorLocation = -1;

        // ?ｌ? ?뚮뜑留??곗씠???꾨줈洹몃옩??而댄뙆?쇳븯怨?uniform ?꾩튂瑜?議고쉶?쒕떎.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwGroundShadowShader : GlfwShader {
        GLint   shadowColorLocation = -1;

        // 吏硫?洹몃┝???곗씠???꾨줈洹몃옩??而댄뙆?쇳븯怨?uniform ?꾩튂瑜?議고쉶?쒕떎.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwViewerMaterial : ViewerMaterial {
        GLuint  texture = 0;
        bool    textureHasAlpha = false;
        GLuint  sphereTexture = 0;
        GLuint  toonTexture = 0;

        explicit GlfwViewerMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
    };

    struct GlfwViewerInfo : ViewerInfo {
        GLuint                                      dummyColorTex = 0;
        std::unique_ptr<GlfwModelShader>            shader;
        std::unique_ptr<GlfwEdgeShader>             edgeShader;
        std::unique_ptr<GlfwGroundShadowShader>     gsShader;
    };

    class GlfwViewer : public Viewer {
        static void* LoadGlProc(const char* name) {
            return reinterpret_cast<void*>(glfwGetProcAddress(name));
        }

        const int           msaaSamples = 4;
        GlfwTextureCache    textureCache;

    public:
        GlfwViewer();
        ~GlfwViewer() override;

        GlfwViewerInfo& GetGlfwInfo() { return static_cast<GlfwViewerInfo&>(GetInfo()); }
        const GlfwViewerInfo& GetGlfwInfo() const { return static_cast<const GlfwViewerInfo&>(GetInfo()); }

        // OpenGL ?뚮뜑留곸뿉 ?꾩슂??GLFW ?덈룄???뚰듃瑜??ㅼ젙?쒕떎.
        void ConfigureGlfwHints() override;
        // OpenGL 而⑦뀓?ㅽ듃? ?곗씠?? 湲곕낯 ?띿뒪泥섎? 珥덇린?뷀븳??
        bool Setup() override;
        // 李??ш린??留욎떠 OpenGL 酉고룷?몄? ?ъ쁺 ?됰젹??媛깆떊?쒕떎.
        bool Resize() override;
        // 而щ윭/源딆씠 踰꾪띁瑜?吏?곌퀬 ?꾨젅???뚮뜑留곸쓣 ?쒖옉?쒕떎.
        void BeginFrame() override;
        // GLFW 踰꾪띁瑜?援먯껜?섍퀬 ?대깽??泥섎━瑜?吏꾪뻾?쒕떎.
        bool EndFrame() override;
        // OpenGL 紐⑤뜽 ?몄뒪?댁뒪瑜??앹꽦?쒕떎.
        std::unique_ptr<Instance> CreateInstance() const override;

        // ?띿뒪泥섎? 罹먯떆?먯꽌 李얘굅???뚯씪?먯꽌 濡쒕뱶??OpenGL ?띿뒪泥섎줈 諛섑솚?쒕떎.
        GlfwTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
    };
}
