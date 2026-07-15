#include "Viewer/Instance/OpenGlInstance.h"

#include "Viewer/Drawer/OpenGlDrawer.h"
#include "Viewer/Viewer/OpenGlViewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>

namespace Chrivent {
	GLuint OpenGlInstance::CreateBuffer(const size_t size, const void* data, const GLenum usage) {
		GLuint b = 0;
		glCreateBuffers(1, &b);
		glNamedBufferData(b, size, data, usage);
		return b;
	}

	GLuint OpenGlInstance::CreateVao(const GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
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

	bool OpenGlInstance::CreateGeometryBuffers() {
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

	void OpenGlInstance::CreateVertexArrays() {
		const GLint locs[][3] = {
			{ viewer->shader->positionLocation, viewer->shader->normalLocation, viewer->shader->uvLocation },
			{ viewer->edgeShader->positionLocation, viewer->edgeShader->normalLocation },
			{ viewer->gsShader->positionLocation },
			{ viewer->depthOnlyShader->positionLocation, viewer->depthOnlyShader->uvLocation },
			{ viewer->sceneVelocityShader->positionLocation, viewer->sceneVelocityShader->previousPositionLocation,
				viewer->sceneVelocityShader->uvLocation }
		};
		constexpr GLint sizes[][3] = {
			{ 3, 3, 2 },
			{ 3, 3 },
			{ 3 },
			{ 3, 2 },
			{ 3, 3, 2 }
		};
		constexpr size_t offsets[][3] = {
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, normal), offsetof(ViewerVertex, uv) },
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, normal) },
			{ offsetof(ViewerVertex, position) },
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, uv) },
			{ offsetof(ViewerVertex, position), offsetof(ViewerVertex, previousPosition), offsetof(ViewerVertex, uv) }
		};
		vao = CreateVao(vertexVbo, locs[0], sizes[0], offsets[0], 3, ibo);
		edgeVao = CreateVao(vertexVbo, locs[1], sizes[1], offsets[1], 2, ibo);
		gsVao = CreateVao(vertexVbo, locs[2], sizes[2], offsets[2], 1, ibo);
		depthVao = CreateVao(vertexVbo, locs[3], sizes[3], offsets[3], 2, ibo);
		velocityVao = CreateVao(vertexVbo, locs[4], sizes[4], offsets[4], 3, ibo);
	}

	bool OpenGlInstance::SetupConstantRings() {
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
		std::string error;
		if (!vertexConstantsRing.Setup(
			DynamicBufferRing::AlignUp(vertexConstantsSize, this->uniformBufferOffsetAlignment)
				* (drawCount + ringSlack),
			GL_DYNAMIC_DRAW, error))
			return false;
		return pixelConstantsRing.Setup(
			DynamicBufferRing::AlignUp(pixelConstantsSize, this->uniformBufferOffsetAlignment)
				* (drawCount * 2 + ringSlack),
			GL_DYNAMIC_DRAW, error);
	}

	void OpenGlInstance::LoadMaterials() {
		for (const auto& mat : model->materialData.materials) {
			OpenGlMaterial material(mat);
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

	OpenGlInstance::OpenGlInstance(OpenGlViewer& sourceViewer) : viewer(&sourceViewer) {
		drawer = std::make_unique<OpenGlDrawer>(*this);
	}

	OpenGlInstance::~OpenGlInstance() {
		OpenGlInstance::Clear();
	}
	
	void OpenGlInstance::Clear() {
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
		if (depthVao != 0)
			glDeleteVertexArrays(1, &depthVao);
		if (velocityVao != 0)
			glDeleteVertexArrays(1, &velocityVao);
		vao = edgeVao = gsVao = depthVao = velocityVao = 0;
		uniformBufferOffsetAlignment = 1;
		materials.clear();
	}

	bool OpenGlInstance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return false;
		CreateVertexArrays();
		if (!SetupConstantRings())
			return false;
		LoadMaterials();
		return true;
	}

	void OpenGlInstance::Upload() const {
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
