#include "GlfwInstance.h"

#include "GlfwDrawer.h"
#include "GlfwViewer.h"
#include "../../../Model/ModelPose.h"
#include "../GlslShaderConstants.h"

#include <algorithm>
#include <string>

namespace Chrivent {
	GLuint GlfwInstance::CreateBuffer(const GLenum target, const size_t size, const void* data, const GLenum usage) {
		GLuint b = 0;
		glGenBuffers(1, &b);
		glBindBuffer(target, b);
		glBufferData(target, size, data, usage);
		return b;
	}

	GLuint GlfwInstance::CreateVao(const GLuint* buffers, const GLint* locs, const GLint* sizes, const GLenum* types,
		const int attribCount, const GLuint ibo) {
		GLuint vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		for (int i = 0; i < attribCount; i++) {
			if (locs[i] < 0)
				continue;
			glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
			glVertexAttribPointer(locs[i], sizes[i], types[i], GL_FALSE, 0, nullptr);
			glEnableVertexAttribArray(locs[i]);
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBindVertexArray(0);
		return vao;
	}

	bool GlfwInstance::CreateGeometryBuffers(GlfwInstanceInfo& info) {
		const size_t vtxCount = info.model->geometryData.positions.size();
		posVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		norVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		uvVbo  = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		const size_t idxSize = info.model->geometryData.indexElementSize;
		const size_t idxCount = info.model->geometryData.indexCount;
		ibo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, idxSize * idxCount, info.model->geometryData.indices.data(), GL_STATIC_DRAW);
		if (idxSize == 1)
			info.indexType = GL_UNSIGNED_BYTE;
		else if (idxSize == 2)
			info.indexType = GL_UNSIGNED_SHORT;
		else if (idxSize == 4)
			info.indexType = GL_UNSIGNED_INT;
		else
			return false;
		return true;
	}

	void GlfwInstance::CreateVertexArrays(GlfwInstanceInfo& info) const {
		const GLuint buffers[][3] = {
			{ posVbo, norVbo, uvVbo },
			{ posVbo, norVbo },
			{ posVbo }
		};
		const GLint locs[][3] = {
			{ info.viewer->GetGlfwInfo().shader->positionLocation, info.viewer->GetGlfwInfo().shader->normalLocation, info.viewer->GetGlfwInfo().shader->uvLocation },
			{ info.viewer->GetGlfwInfo().edgeShader->positionLocation, info.viewer->GetGlfwInfo().edgeShader->normalLocation },
			{ info.viewer->GetGlfwInfo().gsShader->positionLocation }
		};
		constexpr GLint sizes[][3] = {
			{ 3, 3, 2 },
			{ 3, 3 },
			{ 3 }
		};
		constexpr GLenum types[][3]  = {
			{ GL_FLOAT, GL_FLOAT, GL_FLOAT },
			{ GL_FLOAT, GL_FLOAT },
			{ GL_FLOAT }
		};
		info.vao = CreateVao(buffers[0], locs[0], sizes[0], types[0], 3, ibo);
		info.edgeVao = CreateVao(buffers[1], locs[1], sizes[1], types[1], 2, ibo);
		info.gsVao = CreateVao(buffers[2], locs[2], sizes[2], types[2], 1, ibo);
	}

	bool GlfwInstance::SetupConstantRings(GlfwInstanceInfo& info) {
		constexpr size_t vertexConstantsSize = std::max({
			sizeof(ModelVertexConstants),
			sizeof(EdgeVertexConstants),
			sizeof(GroundShadowVertexConstants)
		});
		constexpr size_t pixelConstantsSize = std::max({
			sizeof(ModelPixelConstants),
			sizeof(EdgePixelConstants),
			sizeof(GroundShadowPixelConstants)
		});
		GLint uniformBufferOffsetAlignment = 1;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniformBufferOffsetAlignment);
		info.uniformBufferOffsetAlignment = std::max(1, uniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, info.model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		const auto AlignedSize = [&](const size_t size) {
			const size_t alignment = info.uniformBufferOffsetAlignment;
			const size_t remainder = size % alignment;
			if (remainder == 0)
				return size;
			return size + (alignment - remainder);
		};
		std::string error;
		if (!info.vertexConstantsRing.Setup(
			GL_UNIFORM_BUFFER,
			AlignedSize(vertexConstantsSize) * (drawCount + ringSlack),
			GL_DYNAMIC_DRAW,
			error))
			return false;
		return info.pixelConstantsRing.Setup(
			GL_UNIFORM_BUFFER,
			AlignedSize(pixelConstantsSize) * (drawCount + ringSlack),
			GL_DYNAMIC_DRAW,
			error);
	}

	void GlfwInstance::LoadMaterials(GlfwInstanceInfo& info) {
		for (const auto& mat : info.model->materialData.materials) {
			GlfwViewerMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto texture = info.viewer->LoadTexture(mat.texture);
				material.texture = texture.texture;
				material.textureHasAlpha = texture.hasAlpha;
			}
			if (!mat.spTexture.empty())
				material.sphereTexture = info.viewer->LoadTexture(mat.spTexture).texture;
			if (!mat.toonTexture.empty())
				material.toonTexture = info.viewer->LoadTexture(mat.toonTexture, true).texture;
			info.materials.emplace_back(material);
		}
	}

	GlfwInstance::GlfwInstance() {
		info = std::make_unique<GlfwInstanceInfo>();
	}

	GlfwInstance::~GlfwInstance() {
		GlfwInstance::Clear();
	}
	
	void GlfwInstance::Clear() {
		if (posVbo != 0)
			glDeleteBuffers(1, &posVbo);
		if (norVbo != 0)
			glDeleteBuffers(1, &norVbo);
		if (uvVbo != 0)
			glDeleteBuffers(1, &uvVbo);
		if (ibo != 0)
			glDeleteBuffers(1, &ibo);
		posVbo = norVbo = uvVbo = ibo = 0;
		auto& info = static_cast<GlfwInstanceInfo&>(GetInfo());
		info.vertexConstantsRing.Clear();
		info.pixelConstantsRing.Clear();
		if (info.vao != 0)
			glDeleteVertexArrays(1, &info.vao);
		if (info.edgeVao != 0)
			glDeleteVertexArrays(1, &info.edgeVao);
		if (info.gsVao != 0)
			glDeleteVertexArrays(1, &info.gsVao);
		info.vao = info.edgeVao = info.gsVao = 0;
		info.uniformBufferOffsetAlignment = 1;
	}

	bool GlfwInstance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<GlfwInstanceInfo&>(GetInfo());
		info.viewer = &static_cast<GlfwViewer&>(baseViewer);
		if (info.model == nullptr)
			return false;
		drawer = std::make_unique<GlfwDrawer>(info);
		if (!CreateGeometryBuffers(info))
			return false;
		CreateVertexArrays(info);
		if (!SetupConstantRings(info))
			return false;
		LoadMaterials(info);
		return true;
	}

	void GlfwInstance::Update() const {
		const auto& info = static_cast<const GlfwInstanceInfo&>(GetInfo());
		const ModelPose pose(*info.model);
		pose.Update();
		const size_t vtxCount = info.model->geometryData.positions.size();
		glBindBuffer(GL_ARRAY_BUFFER, posVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * vtxCount,
			info.model->geometryData.updatePositions.data());
		glBindBuffer(GL_ARRAY_BUFFER, norVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * vtxCount,
			info.model->geometryData.updateNormals.data());
		glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec2) * vtxCount,
			info.model->geometryData.updateUVs.data());
	}
}
