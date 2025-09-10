#pragma once

#include <glad/glad.h>

#include "MMDModel.h"

struct Material
{
    explicit Material(const saba::MMDMaterial& mat)
        : m_mmdMat(mat)
    {
    }

    const saba::MMDMaterial& m_mmdMat;
    GLuint	m_texture = 0;
    bool	m_textureHasAlpha = false;
    GLuint	m_spTexture = 0;
    GLuint	m_toonTexture = 0;
};

struct AppContext;

struct Model
{
    std::shared_ptr<saba::MMDModel>	m_mmdModel;
    std::unique_ptr<saba::VMDAnimation>	m_vmdAnim;

    GLuint	m_posVBO = 0;
    GLuint	m_norVBO = 0;
    GLuint	m_uvVBO = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType;

    GLuint	m_mmdVAO = 0;
    GLuint	m_mmdEdgeVAO = 0;
    GLuint	m_mmdGroundShadowVAO = 0;

    std::vector<Material>	m_materials;

    bool Setup(AppContext& appContext);
    void Clear();

    void UpdateAnimation(const AppContext& appContext);
    void Update(const AppContext& appContext);
    void Draw(const AppContext& appContext);
};
