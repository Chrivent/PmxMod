#include "Viewer/Instance/OpenGlInstance.h"

#include "Viewer/Drawer/OpenGlDrawer.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/Texture/OpenGlTextureCache.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace Chrivent {
	GLuint OpenGlInstance::CreateBuffer(const size_t size, const void* data, const GLenum usage) {
		GLuint b = 0;
		glCreateBuffers(1, &b);
		if (b == 0)
			return 0;
		glNamedBufferData(b, size, data, usage);
		return b;
	}

	GLuint OpenGlInstance::CreateVao(const GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
		const size_t* offsets, const int attributeCount, const GLuint indexBuffer) {
		if (vertexBuffer == 0 || indexBuffer == 0)
			return 0;
		GLuint vao = 0;
		glCreateVertexArrays(1, &vao);
		if (vao == 0)
			return 0;
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
		if (vertexVbo == 0 || ibo == 0)
			return false;
		if (idxSize == 1)
			modelResources.indexType = GL_UNSIGNED_BYTE;
		else if (idxSize == 2)
			modelResources.indexType = GL_UNSIGNED_SHORT;
		else if (idxSize == 4)
			modelResources.indexType = GL_UNSIGNED_INT;
		else
			return false;
		return true;
	}

	bool OpenGlInstance::CreateVertexArrays() {
		const OpenGlSceneAttributeLocations locations = drawContext.ResolveSceneAttributeLocations();
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
		modelResources.vao = CreateVao(vertexVbo, locations.model, sizes[0], offsets[0], 3, ibo);
		modelResources.edgeVao = CreateVao(vertexVbo, locations.edge, sizes[1], offsets[1], 2, ibo);
		modelResources.gsVao = CreateVao(vertexVbo, locations.groundShadow, sizes[2], offsets[2], 1, ibo);
		modelResources.depthVao = CreateVao(vertexVbo, locations.depth, sizes[3], offsets[3], 2, ibo);
		modelResources.velocityVao = CreateVao(vertexVbo, locations.velocity, sizes[4], offsets[4], 3, ibo);
		return modelResources.vao != 0 && modelResources.edgeVao != 0 && modelResources.gsVao != 0
			&& modelResources.depthVao != 0 && modelResources.velocityVao != 0;
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
		modelResources.uniformBufferOffsetAlignment = std::max(1, uniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		const size_t vertexUploadCount = drawCount + 3;
		const size_t pixelUploadCount = drawCount * 3 + 1;
		std::string error;
		if (!modelResources.vertexConstantsRing.Setup(
			DynamicBufferRing::AlignUp(vertexConstantsSize, modelResources.uniformBufferOffsetAlignment)
				* vertexUploadCount,
			GL_DYNAMIC_DRAW, error))
			return false;
		return modelResources.pixelConstantsRing.Setup(
			DynamicBufferRing::AlignUp(pixelConstantsSize, modelResources.uniformBufferOffsetAlignment)
				* pixelUploadCount,
			GL_DYNAMIC_DRAW, error);
	}

	void OpenGlInstance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			OpenGlModelMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto [hasAlpha, texture] = textureCache.Load(mat.texture);
				material.texture = texture;
				material.textureHasAlpha = hasAlpha;
			}
			if (!mat.spTexture.empty())
				material.sphereTexture = textureCache.Load(mat.spTexture).texture;
			if (!mat.toonTexture.empty())
				material.toonTexture = textureCache.Load(mat.toonTexture, true).texture;
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	OpenGlInstance::OpenGlInstance(OpenGlTextureCache& sourceTextureCache,
		OpenGlDrawContext& sourceDrawContext)
		: Instance(GraphicsApi::OpenGl), textureCache(sourceTextureCache),
		drawContext(sourceDrawContext) {
		drawer = std::make_unique<OpenGlDrawer>(*this, modelResources, drawContext);
	}

	OpenGlInstance::~OpenGlInstance() {
		OpenGlInstance::ResetRendererResources();
	}

	void OpenGlInstance::ResetRendererResources() {
		if (vertexVbo != 0)
			glDeleteBuffers(1, &vertexVbo);
		if (ibo != 0)
			glDeleteBuffers(1, &ibo);
		vertexVbo = ibo = 0;
		modelResources.vertexConstantsRing.Clear();
		modelResources.pixelConstantsRing.Clear();
		if (modelResources.vao != 0)
			glDeleteVertexArrays(1, &modelResources.vao);
		if (modelResources.edgeVao != 0)
			glDeleteVertexArrays(1, &modelResources.edgeVao);
		if (modelResources.gsVao != 0)
			glDeleteVertexArrays(1, &modelResources.gsVao);
		if (modelResources.depthVao != 0)
			glDeleteVertexArrays(1, &modelResources.depthVao);
		if (modelResources.velocityVao != 0)
			glDeleteVertexArrays(1, &modelResources.velocityVao);
		modelResources.vao = modelResources.edgeVao = modelResources.gsVao = 0;
		modelResources.depthVao = modelResources.velocityVao = 0;
		modelResources.uniformBufferOffsetAlignment = 1;
		modelResources.materials.clear();
	}

	GraphicsResult<void> OpenGlInstance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"OpenGL 모델 인스턴스 초기화", "vertex 또는 index buffer를 만들지 못했습니다"));
		if (!CreateVertexArrays())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"OpenGL 모델 인스턴스 초기화", "모델 패스용 VAO를 만들지 못했습니다"));
		if (!SetupConstantRings())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"OpenGL 모델 인스턴스 초기화", "uniform buffer ring을 만들지 못했습니다"));
		LoadMaterials();
		return {};
	}

	GraphicsResult<void> OpenGlInstance::UploadCore() {
		const size_t vtxCount = model->geometryData.positions.size();
		auto* vertices = static_cast<ViewerVertex*>(glMapNamedBufferRange(
			vertexVbo, 0, sizeof(ViewerVertex) * vtxCount, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
		if (vertices == nullptr)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"OpenGL 모델 정점 업로드", "vertex buffer를 매핑하지 못했습니다"));
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ vertices, vtxCount });
		const bool unmapSucceeded = glUnmapNamedBuffer(vertexVbo) == GL_TRUE;
		if (!writeSucceeded || !unmapSucceeded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"OpenGL 모델 정점 업로드", "vertex 데이터를 기록하거나 buffer 매핑을 해제하지 못했습니다"));
		return {};
	}
}
