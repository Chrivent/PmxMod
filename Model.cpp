#include "Model.h"

#include "AppContext.h"
#include "MMDShader.h"

Material::Material(const saba::MMDMaterial &mat)
	: m_mmdMat(mat) {
}

bool Model::Setup(AppContext& appContext) {
	if (m_mmdModel == nullptr)
		return false;

	// Setup vertices
	const size_t vtxCount = m_mmdModel->m_positions.size();
	glGenBuffers(1, &m_posVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &m_norVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &m_uvVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const size_t idxSize = m_mmdModel->m_indexElementSize;
	const size_t idxCount = m_mmdModel->m_indexCount;
	glGenBuffers(1, &m_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idxSize * idxCount), &m_mmdModel->m_indices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	if (idxSize == 1)
		m_indexType = GL_UNSIGNED_BYTE;
	else if (idxSize == 2)
		m_indexType = GL_UNSIGNED_SHORT;
	else if (idxSize == 4)
		m_indexType = GL_UNSIGNED_INT;
	else
		return false;

	// Setup MMD VAO
	glGenVertexArrays(1, &m_mmdVAO);
	glBindVertexArray(m_mmdVAO);

	const auto &mmdShader = appContext.m_mmdShader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inPos);

	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glVertexAttribPointer(mmdShader->m_inNor, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inNor);

	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glVertexAttribPointer(mmdShader->m_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inUV);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

	glBindVertexArray(0);

	// Setup MMD Edge VAO
	glGenVertexArrays(1, &m_mmdEdgeVAO);
	glBindVertexArray(m_mmdEdgeVAO);

	const auto &mmdEdgeShader = appContext.m_mmdEdgeShader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdEdgeShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdEdgeShader->m_inPos);

	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glVertexAttribPointer(mmdEdgeShader->m_inNor, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdEdgeShader->m_inNor);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

	glBindVertexArray(0);

	// Setup MMD Ground Shadow VAO
	glGenVertexArrays(1, &m_mmdGroundShadowVAO);
	glBindVertexArray(m_mmdGroundShadowVAO);

	const auto &mmdGroundShadowShader = appContext.m_mmdGroundShadowShader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdGroundShadowShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdGroundShadowShader->m_inPos);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

	glBindVertexArray(0);

	// Setup materials
	for (const auto & mmdMat : m_mmdModel->m_materials) {
		Material mat(mmdMat);
		if (!mmdMat.m_texture.empty()) {
			auto [m_texture, m_hasAlpha] = appContext.GetTexture(mmdMat.m_texture);
			mat.m_texture = m_texture;
			mat.m_textureHasAlpha = m_hasAlpha;
		}
		if (!mmdMat.m_spTexture.empty()) {
			auto [m_texture, m_hasAlpha] = appContext.GetTexture(mmdMat.m_spTexture);
			mat.m_spTexture = m_texture;
		}
		if (!mmdMat.m_toonTexture.empty()) {
			auto [m_texture, m_hasAlpha] = appContext.GetTexture(mmdMat.m_toonTexture);
			mat.m_toonTexture = m_texture;
		}
		m_materials.emplace_back(mat);
	}

	return true;
}

void Model::Clear() {
	if (m_posVBO != 0) glDeleteBuffers(1, &m_posVBO);
	if (m_norVBO != 0) glDeleteBuffers(1, &m_norVBO);
	if (m_uvVBO != 0) glDeleteBuffers(1, &m_uvVBO);
	if (m_ibo != 0) glDeleteBuffers(1, &m_ibo);
	m_posVBO = 0;
	m_norVBO = 0;
	m_uvVBO = 0;
	m_ibo = 0;

	if (m_mmdVAO != 0) glDeleteVertexArrays(1, &m_mmdVAO);
	if (m_mmdEdgeVAO != 0) glDeleteVertexArrays(1, &m_mmdEdgeVAO);
	if (m_mmdGroundShadowVAO != 0) glDeleteVertexArrays(1, &m_mmdGroundShadowVAO);
	m_mmdVAO = 0;
	m_mmdEdgeVAO = 0;
	m_mmdGroundShadowVAO = 0;
}

void Model::UpdateAnimation(const AppContext& appContext) const {
	m_mmdModel->BeginAnimation();
	m_mmdModel->UpdateAllAnimation(m_vmdAnim.get(), appContext.m_animTime * 30.0f, appContext.m_elapsed);
}

void Model::Update() const {
	m_mmdModel->Update();

	const size_t vtxCount = m_mmdModel->m_positions.size();
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), m_mmdModel->m_updatePositions.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), m_mmdModel->m_updateNormals.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount), m_mmdModel->m_updateUVs.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Model::Draw(const AppContext& appContext) const {
	const auto &view = appContext.m_viewMat;
	const auto &proj = appContext.m_projMat;

	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = proj * view * world;

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, appContext.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, appContext.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, appContext.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 6);
	glBindTexture(GL_TEXTURE_2D, appContext.m_dummyShadowDepthTex);

	glEnable(GL_DEPTH_TEST);

	// Draw model
	size_t subMeshCount = m_mmdModel->m_subMeshes.size();
	for (size_t i = 0; i < subMeshCount; i++) {
		const auto &[m_beginIndex, m_vertexCount, m_materialID] = m_mmdModel->m_subMeshes[i];
		const auto &shader = appContext.m_mmdShader;
		const auto &mat = m_materials[m_materialID];
		const auto &mmdMat = mat.m_mmdMat;

		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;

		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdVAO);

		glUniformMatrix4fv(shader->m_uWV, 1, GL_FALSE, &wv[0][0]);
		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);

		glUniform3fv(shader->m_uAmbient, 1, &mmdMat.m_ambient[0]);
		glUniform3fv(shader->m_uDiffuse, 1, &mmdMat.m_diffuse[0]);
		glUniform3fv(shader->m_uSpecular, 1, &mmdMat.m_specular[0]);
		glUniform1f(shader->m_uSpecularPower, mmdMat.m_specularPower);
		glUniform1f(shader->m_uAlpha, mmdMat.m_diffuse.a);

		glActiveTexture(GL_TEXTURE0 + 0);
		glUniform1i(shader->m_uTex, 0);
		if (mat.m_texture != 0) {
			if (!mat.m_textureHasAlpha)
				// Use Material Alpha
					glUniform1i(shader->m_uTexMode, 1);
			else
				// Use Material Alpha * Texture Alpha
					glUniform1i(shader->m_uTexMode, 2);
			glUniform4fv(shader->m_uTexMulFactor, 1, &mmdMat.m_textureMulFactor[0]);
			glUniform4fv(shader->m_uTexAddFactor, 1, &mmdMat.m_textureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, mat.m_texture);
		} else {
			glUniform1i(shader->m_uTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, appContext.m_dummyColorTex);
		}

		glActiveTexture(GL_TEXTURE0 + 1);
		glUniform1i(shader->m_uSphereTex, 1);
		if (mat.m_spTexture != 0) {
			if (mmdMat.m_spTextureMode == saba::PMXSphereMode::Mul)
				glUniform1i(shader->m_uSphereTexMode, 1);
			else if (mmdMat.m_spTextureMode == saba::PMXSphereMode::Add)
				glUniform1i(shader->m_uSphereTexMode, 2);
			glUniform4fv(shader->m_uSphereTexMulFactor, 1, &mmdMat.m_spTextureMulFactor[0]);
			glUniform4fv(shader->m_uSphereTexAddFactor, 1, &mmdMat.m_spTextureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, mat.m_spTexture);
		} else {
			glUniform1i(shader->m_uSphereTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, appContext.m_dummyColorTex);
		}

		glActiveTexture(GL_TEXTURE0 + 2);
		glUniform1i(shader->m_uToonTex, 2);
		if (mat.m_toonTexture != 0) {
			glUniform4fv(shader->m_uToonTexMulFactor, 1, &mmdMat.m_toonTextureMulFactor[0]);
			glUniform4fv(shader->m_uToonTexAddFactor, 1, &mmdMat.m_toonTextureAddFactor[0]);
			glUniform1i(shader->m_uToonTexMode, 1);
			glBindTexture(GL_TEXTURE_2D, mat.m_toonTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} else {
			glUniform1i(shader->m_uToonTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, appContext.m_dummyColorTex);
		}

		glm::vec3 lightColor = appContext.m_lightColor;
		glm::vec3 lightDir = appContext.m_lightDir;
		auto viewMat = glm::mat3(appContext.m_viewMat);
		lightDir = viewMat * lightDir;
		glUniform3fv(shader->m_uLightDir, 1, &lightDir[0]);
		glUniform3fv(shader->m_uLightColor, 1, &lightColor[0]);

		if (mmdMat.m_bothFace)
			glDisable(GL_CULL_FACE);
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUniform1i(shader->m_uShadowMapEnabled, 0);
		glUniform1i(shader->m_uShadowMap0, 3);
		glUniform1i(shader->m_uShadowMap1, 4);
		glUniform1i(shader->m_uShadowMap2, 5);
		glUniform1i(shader->m_uShadowMap3, 6);

		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));

		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glUseProgram(0);
	}

	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 6);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Draw edge
	glm::vec2 screenSize(appContext.m_screenWidth, appContext.m_screenHeight);
	for (size_t i = 0; i < subMeshCount; i++) {
		const auto &[m_beginIndex, m_vertexCount, m_materialID] = m_mmdModel->m_subMeshes[i];
		const auto &shader = appContext.m_mmdEdgeShader;
		const auto &mat = m_materials[m_materialID];
		const auto &mmdMat = mat.m_mmdMat;

		if (!mmdMat.m_edgeFlag)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;

		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdEdgeVAO);

		glUniformMatrix4fv(shader->m_uWV, 1, GL_FALSE, &wv[0][0]);
		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);
		glUniform2fv(shader->m_uScreenSize, 1, &screenSize[0]);
		glUniform1f(shader->m_uEdgeSize, mmdMat.m_edgeSize);
		glUniform4fv(shader->m_uEdgeColor, 1, &mmdMat.m_edgeColor[0]);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));

		glBindVertexArray(0);
		glUseProgram(0);
	}

	// Draw ground shadow
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);
	auto plane = glm::vec4(0, 1, 0, 0);
	auto light = -appContext.m_lightDir;
	auto shadow = glm::mat4(1);

	shadow[0][0] = plane.y * light.y + plane.z * light.z;
	shadow[0][1] = -plane.x * light.y;
	shadow[0][2] = -plane.x * light.z;
	shadow[0][3] = 0;

	shadow[1][0] = -plane.y * light.x;
	shadow[1][1] = plane.x * light.x + plane.z * light.z;
	shadow[1][2] = -plane.y * light.z;
	shadow[1][3] = 0;

	shadow[2][0] = -plane.z * light.x;
	shadow[2][1] = -plane.z * light.y;
	shadow[2][2] = plane.x * light.x + plane.y * light.y;
	shadow[2][3] = 0;

	shadow[3][0] = -plane.w * light.x;
	shadow[3][1] = -plane.w * light.y;
	shadow[3][2] = -plane.w * light.z;
	shadow[3][3] = plane.x * light.x + plane.y * light.y + plane.z * light.z;

	auto wsvp = proj * view * shadow * world;

	auto shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	if (shadowColor.a < 1.0f) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glEnable(GL_STENCIL_TEST);
	} else
		glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	for (size_t i = 0; i < subMeshCount; i++) {
		const auto &[m_beginIndex, m_vertexCount, m_materialID] = m_mmdModel->m_subMeshes[i];
		const auto &mat = m_materials[m_materialID];
		const auto &mmdMat = mat.m_mmdMat;
		const auto &shader = appContext.m_mmdGroundShadowShader;

		if (!mmdMat.m_groundShadow)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;

		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdGroundShadowVAO);

		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wsvp[0][0]);
		glUniform4fv(shader->m_uShadowColor, 1, &shadowColor[0]);

		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));

		glBindVertexArray(0);
		glUseProgram(0);
	}

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
}
