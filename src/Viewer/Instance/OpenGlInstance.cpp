#include "Viewer/Instance/OpenGlInstance.h"

#include "Viewer/Drawer/OpenGlDrawer.h"
#include "Viewer/Buffer/BufferSize.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/Texture/OpenGlTextureCache.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace Chrivent {
	GraphicsResult<GLuint> OpenGlInstance::CreateBuffer(
		const size_t size, const void* data, const GLenum usage) const {
		if (size == 0 || size > static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max())) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL buffer 생성", "buffer 크기가 OpenGL 범위를 벗어났습니다"));
		}
		GLuint b = 0;
		glCreateBuffers(1, &b);
		GLenum result = glGetError();
		if (b == 0 || result != GL_NO_ERROR) {
			if (b != 0)
				glDeleteBuffers(1, &b);
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"OpenGL buffer 생성", "buffer object를 만들지 못했습니다",
				result, result != GL_NO_ERROR));
		}
		glNamedBufferData(b, size, data, usage);
		result = glGetError();
		if (result == GL_NO_ERROR)
			return b;
		glDeleteBuffers(1, &b);
		return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
			"OpenGL buffer storage 생성", "buffer storage를 할당하지 못했습니다", result, true));
	}

	GraphicsResult<GLuint> OpenGlInstance::CreateVao(
		const GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
		const size_t* offsets, const int attributeCount, const GLuint indexBuffer) const {
		if (vertexBuffer == 0 || indexBuffer == 0 || locations == nullptr
			|| sizes == nullptr || offsets == nullptr || attributeCount <= 0) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL VAO 생성", "buffer 또는 attribute 정보가 올바르지 않습니다"));
		}
		GLuint vao = 0;
		glCreateVertexArrays(1, &vao);
		GLenum result = glGetError();
		if (vao == 0 || result != GL_NO_ERROR) {
			if (vao != 0)
				glDeleteVertexArrays(1, &vao);
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"OpenGL VAO 생성", "vertex array object를 만들지 못했습니다",
				result, result != GL_NO_ERROR));
		}
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
		result = glGetError();
		if (result == GL_NO_ERROR)
			return vao;
		glDeleteVertexArrays(1, &vao);
		return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
			"OpenGL VAO 구성", "vertex attribute 또는 index buffer를 연결하지 못했습니다", result, true));
	}

	GraphicsResult<void> OpenGlInstance::CreateGeometryBuffers() {
		const size_t vtxCount = model->geometryData.positions.size();
		const size_t idxSize = model->geometryData.indexElementSize;
		const size_t idxCount = model->geometryData.indexCount;
		size_t vertexByteSize = 0;
		size_t indexByteSize = 0;
		if (!BufferSize::TryMultiply(sizeof(ViewerVertex), vtxCount, vertexByteSize)
			|| !BufferSize::TryMultiply(idxSize, idxCount, indexByteSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL geometry 생성", "vertex 또는 index buffer 크기가 한도를 넘습니다"));
		}
		const auto vertexResult = CreateBuffer(vertexByteSize, nullptr, GL_DYNAMIC_DRAW);
		if (!vertexResult)
			return std::unexpected(vertexResult.error());
		vertexVbo = *vertexResult;
		const auto indexResult = CreateBuffer(
			indexByteSize, model->geometryData.indices.data(), GL_STATIC_DRAW);
		if (!indexResult)
			return std::unexpected(indexResult.error());
		ibo = *indexResult;
		if (idxSize == 1)
			modelResources.indexType = GL_UNSIGNED_BYTE;
		else if (idxSize == 2)
			modelResources.indexType = GL_UNSIGNED_SHORT;
		else if (idxSize == 4)
			modelResources.indexType = GL_UNSIGNED_INT;
		else {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL geometry 생성", "index element 크기가 올바르지 않습니다"));
		}
		return {};
	}

	GraphicsResult<void> OpenGlInstance::CreateVertexArrays() {
		constexpr GLint locations[][3] = {
			{ 0, 1, 2 },
			{ 0, 1, -1 },
			{ 0, -1, -1 },
			{ 0, 1, -1 },
			{ 0, 1, 2 }
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
		GLuint* vaos[] = {
			&modelResources.vao,
			&modelResources.edgeVao,
			&modelResources.gsVao,
			&modelResources.depthVao,
			&modelResources.velocityVao
		};
		for (size_t index = 0; index < std::size(vaos); index++) {
			constexpr int attributeCounts[] = { 3, 2, 1, 2, 3 };
			const auto result = CreateVao(
				vertexVbo, locations[index], sizes[index], offsets[index], attributeCounts[index], ibo);
			if (!result)
				return std::unexpected(result.error());
			*vaos[index] = *result;
		}
		return {};
	}

	GraphicsResult<void> OpenGlInstance::SetupConstantRings() {
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
		const GLenum alignmentResult = glGetError();
		if (alignmentResult != GL_NO_ERROR) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"OpenGL uniform buffer 정렬 조회",
				"uniform buffer offset 정렬을 조회하지 못했습니다", alignmentResult, true));
		}
		modelResources.uniformBufferOffsetAlignment = std::max(1, uniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		size_t vertexUploadCount = 0;
		size_t pixelUploadCount = 0;
		size_t alignedVertexSize = 0;
		size_t alignedPixelSize = 0;
		size_t vertexCapacity = 0;
		size_t pixelCapacity = 0;
		if (!BufferSize::TryAdd(drawCount, 3, vertexUploadCount)
			|| !BufferSize::TryMultiply(drawCount, 3, pixelUploadCount)
			|| !BufferSize::TryAdd(pixelUploadCount, 1, pixelUploadCount)
			|| !BufferSize::TryAlignUp(vertexConstantsSize,
				modelResources.uniformBufferOffsetAlignment, alignedVertexSize)
			|| !BufferSize::TryAlignUp(pixelConstantsSize,
				modelResources.uniformBufferOffsetAlignment, alignedPixelSize)
			|| !BufferSize::TryMultiply(alignedVertexSize, vertexUploadCount, vertexCapacity)
			|| !BufferSize::TryMultiply(alignedPixelSize, pixelUploadCount, pixelCapacity)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL uniform buffer ring 크기 계산",
				"material 수에 따른 uniform buffer ring 크기가 한도를 넘습니다"));
		}
		auto result = modelResources.vertexConstantsRing.Setup(vertexCapacity, GL_DYNAMIC_DRAW);
		if (!result)
			return std::unexpected(result.error());
		result = modelResources.pixelConstantsRing.Setup(pixelCapacity, GL_DYNAMIC_DRAW);
		if (!result)
			return std::unexpected(result.error());
		return {};
	}

	GraphicsResult<void> OpenGlInstance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			OpenGlModelMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto textureResult = textureCache.Load(mat.texture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult) {
					material.texture = (*textureResult)->texture;
					material.textureHasAlpha = (*textureResult)->hasAlpha;
				}
			}
			if (!mat.spTexture.empty()) {
				const auto textureResult = textureCache.Load(mat.spTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.sphereTexture = (*textureResult)->texture;
			}
			if (!mat.toonTexture.empty()) {
				const auto textureResult = textureCache.Load(mat.toonTexture, true);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.toonTexture = (*textureResult)->texture;
			}
			modelResources.materials.emplace_back(std::move(material));
		}
		return {};
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
		auto result = CreateGeometryBuffers();
		if (!result)
			return std::unexpected(result.error());
		result = CreateVertexArrays();
		if (!result)
			return std::unexpected(result.error());
		result = SetupConstantRings();
		if (!result)
			return std::unexpected(result.error());
		return LoadMaterials();
	}

	GraphicsResult<void> OpenGlInstance::UploadCore() {
		const size_t vtxCount = model->geometryData.positions.size();
		size_t vertexByteSize = 0;
		if (!BufferSize::TryMultiply(sizeof(ViewerVertex), vtxCount, vertexByteSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"OpenGL 모델 정점 업로드", "vertex buffer 크기가 한도를 넘습니다"));
		}
		auto* vertices = static_cast<ViewerVertex*>(glMapNamedBufferRange(
			vertexVbo, 0, vertexByteSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
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
