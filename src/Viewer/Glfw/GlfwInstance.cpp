#include "Viewer/Glfw/GlfwInstance.h"

#include "Viewer/Glfw/GlfwDrawer.h"
#include "Viewer/Glfw/GlfwViewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>

namespace Chrivent {
	GLuint GlfwInstance::CreateBuffer(const size_t size, const void* data, const GLenum usage) {
		GLuint b = 0;
		glCreateBuffers(1, &b);
		glNamedBufferData(b, size, data, usage);
		return b;
	}

	GLuint GlfwInstance::CreateVao(const GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
		const size_t* offsets, const int attributeCount, const GLuint indexBuffer) {
		GLuint vao = 0;
		glCreateVertexArrays(1, &vao);
		glVertexArrayVertexBuffer(vao, 0, vertexBuffer, 0, sizeof(ViewerVertex));
		for (int index = 0; index < attributeCount; index++) {
			if (locations[index] < 0)
				continue;
			glEnableVertexArrayAttrib(vao, locations[index]);
			glVertexArrayAttribFormat(
				vao, locations[index], sizes[index], GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsets[index]));
			glVertexArrayAttribBinding(vao, locations[index], 0);
		}
		glVertexArrayElementBuffer(vao, indexBuffer);
		return vao;
	}

	bool GlfwInstance::CreateGeometryBuffers() {
		const size_t vtxCount = model->geometryData.positions.size();
		vertexVbo = CreateBuffer(sizeof(ViewerVertex) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		const size_t idxSize = model->geometryData.indexElementSize;
		const size_t idxCount = model->geometryData.indexCount;
		ibo = CreateBuffer(idxSize * idxCount, model->geometryData.indices.data(), GL_STATIC_DRAW);
		if (idxSize == 1)
			indexType = GL_UNSIGNED_BYTE;
		else if (idxSize == 2)
			indexType = GL_UNSIGNED_SHORT;
		else if (idxSize == 4)
			indexType = GL_UNSIGNED_INT;
		else
			return false;
		return true;
	}

	void GlfwInstance::CreateVertexArrays() {
		const GLint locs[][3] = {
			{ viewer->shader->positionLocation, viewer->shader->normalLocation, viewer->shader->uvLocation },
			{ viewer->edgeShader->positionLocation, viewer->edgeShader->normalLocation },
			{ viewer->gsShader->positionLocation }
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
		vao = CreateVao(vertexVbo, locs[0], sizes[0], offsets[0], 3, ibo);
		edgeVao = CreateVao(vertexVbo, locs[1], sizes[1], offsets[1], 2, ibo);
		gsVao = CreateVao(vertexVbo, locs[2], sizes[2], offsets[2], 1, ibo);
	}

	bool GlfwInstance::SetupConstantRings() {
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
		this->uniformBufferOffsetAlignment = std::max(1, uniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		const auto AlignedSize = [&](const size_t size) {
			const size_t alignment = this->uniformBufferOffsetAlignment;
			const size_t remainder = size % alignment;
			if (remainder == 0)
				return size;
			return size + (alignment - remainder);
		};
		std::string error;
		if (!vertexConstantsRing.Setup(
			AlignedSize(vertexConstantsSize) * (drawCount + ringSlack),
			GL_DYNAMIC_DRAW, error))
			return false;
		return pixelConstantsRing.Setup(
			AlignedSize(pixelConstantsSize) * (drawCount + ringSlack),
			GL_DYNAMIC_DRAW, error);
	}

	void GlfwInstance::LoadMaterials() {
		for (const auto& mat : model->materialData.materials) {
			GlfwMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto texture = viewer->LoadTexture(mat.texture);
				material.texture = texture.texture;
				material.textureHasAlpha = texture.hasAlpha;
			}
			if (!mat.spTexture.empty())
				material.sphereTexture = viewer->LoadTexture(mat.spTexture).texture;
			if (!mat.toonTexture.empty())
				material.toonTexture = viewer->LoadTexture(mat.toonTexture, true).texture;
			materials.emplace_back(material);
		}
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
		vertexConstantsRing.Clear();
		pixelConstantsRing.Clear();
		if (vao != 0)
			glDeleteVertexArrays(1, &vao);
		if (edgeVao != 0)
			glDeleteVertexArrays(1, &edgeVao);
		if (gsVao != 0)
			glDeleteVertexArrays(1, &gsVao);
		vao = edgeVao = gsVao = 0;
		uniformBufferOffsetAlignment = 1;
	}

	bool GlfwInstance::Setup(Viewer& baseViewer) {
		viewer = &static_cast<GlfwViewer&>(baseViewer);
		if (model == nullptr)
			return false;
		drawer = std::make_unique<GlfwDrawer>(*this);
		if (!CreateGeometryBuffers())
			return false;
		CreateVertexArrays();
		if (!SetupConstantRings())
			return false;
		LoadMaterials();
		return true;
	}

	void GlfwInstance::Upload() const {
		const size_t vtxCount = model->geometryData.positions.size();
		auto* vertices = static_cast<ViewerVertex*>(glMapNamedBufferRange(
			vertexVbo, 0, sizeof(ViewerVertex) * vtxCount, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		if (vertices == nullptr) {
			std::cerr << "Failed to update OpenGL vertex buffers.\n";
			return;
		}
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ vertices, vtxCount });
		const bool unmapSucceeded = glUnmapNamedBuffer(vertexVbo) == GL_TRUE;
		if (!writeSucceeded || !unmapSucceeded)
			std::cerr << "Failed to update OpenGL vertex buffers.\n";
	}
}
