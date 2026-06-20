#include "GlfwInstance.h"

#include "GlfwDrawer.h"
#include "GlfwViewer.h"
#include "../../../Core/Model/Model.h"
#include "../GlslShaderConstants.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>

namespace Chrivent {
	GLuint GlfwInstance::CreateBuffer(const GLenum target, const size_t size, const void* data, const GLenum usage) {
		GLuint b = 0;
		glGenBuffers(1, &b);
		glBindBuffer(target, b);
		glBufferData(target, size, data, usage);
		return b;
	}

	GLuint GlfwInstance::CreateVao(const GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
		const size_t* offsets, const int attributeCount, const GLuint indexBuffer) {
		GLuint vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		for (int index = 0; index < attributeCount; index++) {
			if (locations[index] < 0)
				continue;
			glVertexAttribPointer(
				locations[index],
				sizes[index],
				GL_FLOAT,
				GL_FALSE,
				sizeof(ViewerVertex),
				reinterpret_cast<const void*>(offsets[index]));
			glEnableVertexAttribArray(locations[index]);
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBindVertexArray(0);
		return vao;
	}

	bool GlfwInstance::CreateGeometryBuffers(GlfwInstanceInfo& info) {
		const size_t vtxCount = info.model->geometryData.positions.size();
		vertexVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(ViewerVertex) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
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
		constexpr size_t offsets[][3] = {
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, normal), offsetof(ViewerVertex, uv) },
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, normal) },
			{ offsetof(ViewerVertex, position) }
		};
		info.vao = CreateVao(vertexVbo, locs[0], sizes[0], offsets[0], 3, ibo);
		info.edgeVao = CreateVao(vertexVbo, locs[1], sizes[1], offsets[1], 2, ibo);
		info.gsVao = CreateVao(vertexVbo, locs[2], sizes[2], offsets[2], 1, ibo);
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
		if (vertexVbo != 0)
			glDeleteBuffers(1, &vertexVbo);
		if (ibo != 0)
			glDeleteBuffers(1, &ibo);
		vertexVbo = ibo = 0;
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

	void GlfwInstance::Upload() const {
		const auto& info = static_cast<const GlfwInstanceInfo&>(GetInfo());
		const size_t vtxCount = info.model->geometryData.positions.size();
		glBindBuffer(GL_ARRAY_BUFFER, vertexVbo);
		auto* vertices = static_cast<ViewerVertex*>(glMapBufferRange(
			GL_ARRAY_BUFFER,
			0,
			sizeof(ViewerVertex) * vtxCount,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		if (vertices == nullptr) {
			std::cerr << "Failed to update OpenGL vertex buffers.\n";
			return;
		}
		const bool writeSucceeded = ViewerGeometry::WriteVertices(info.model->geometryData, true, vertices, vtxCount);
		const bool unmapSucceeded = glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE;
		if (!writeSucceeded || !unmapSucceeded)
			std::cerr << "Failed to update OpenGL vertex buffers.\n";
	}
}
