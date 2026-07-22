#include "Viewer/Instance/VulkanInstance.h"

#include "Viewer/Drawer/VulkanDrawer.h"
#include "Viewer/Buffer/BufferSize.h"
#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Texture/VulkanTextureCache.h"
#include "Core/Model/Model.h"

#include <algorithm>
#include <memory>

namespace Chrivent {
	GraphicsError::Result<void> VulkanInstance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan geometry 생성", "모델 index 데이터를 만들지 못했습니다"));
		}
		modelResources.indexType = indexData.elementSize == sizeof(uint16_t)
			? VK_INDEX_TYPE_UINT16
			: VK_INDEX_TYPE_UINT32;
		const size_t vertexCount = geometryData.positions.size();
		size_t vertexBufferSize = 0;
		if (!BufferSize::TryMultiply(sizeof(ViewerVertex), vertexCount, vertexBufferSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan geometry 생성", "vertex buffer 크기가 한도를 넘습니다"));
		}
		const VkDeviceSize indexBufferSize = indexData.bytes.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan geometry 생성", "vertex 또는 index 데이터가 비어 있습니다"));
		}
		for (auto& vertexBuffer : modelResources.vertexBuffers) {
			const auto bufferResult = vertexBuffer.Initialize(
				device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (!bufferResult)
				return std::unexpected(bufferResult.error());
			if (!ViewerGeometry::WriteVertices(geometryData, false,
				{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount })) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan vertex buffer 기록", "초기 vertex 데이터를 기록하지 못했습니다"));
			}
		}
		auto indexUploadBuffer = std::make_unique<VulkanBuffer>();
		auto bufferResult = indexUploadBuffer->Initialize(
			device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (!bufferResult)
			return std::unexpected(bufferResult.error());
		if (!indexUploadBuffer->Write(indexData.bytes.data(), indexBufferSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan index upload buffer 기록", "index 데이터를 upload buffer에 기록하지 못했습니다"));
		}
		bufferResult = modelResources.indexBuffer.Initialize(device, indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!bufferResult)
			return std::unexpected(bufferResult.error());
		return uploadContext.RecordIndexBufferUpload(
			modelResources.indexBuffer.GetBuffer(), std::move(indexUploadBuffer), indexBufferSize);
	}

	GraphicsError::Result<void> VulkanInstance::SetupConstantRings() {
		modelResources.uniformBufferOffsetAlignment = std::max<size_t>(
			1, device.GetUniformBufferAlignment());
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		const auto Align = [this](const size_t size, size_t& result) {
			return BufferSize::TryAlignUp(
				size, modelResources.uniformBufferOffsetAlignment, result);
		};
		const auto AddCapacity = [](const size_t unitSize, const size_t count, size_t& capacity) {
			size_t byteSize = 0;
			return BufferSize::TryMultiply(unitSize, count, byteSize)
				&& BufferSize::TryAdd(capacity, byteSize, capacity);
		};
		size_t alignedModelVertex = 0;
		size_t alignedEdgeVertex = 0;
		size_t alignedGroundShadowVertex = 0;
		size_t alignedModelPixel = 0;
		size_t alignedEdgePixel = 0;
		size_t alignedGroundShadowPixel = 0;
		size_t drawCountWithSlack = 0;
		size_t doubledDrawCountWithSlack = 0;
		size_t vertexFrameCapacity = 0;
		size_t pixelFrameCapacity = 0;
		if (!Align(sizeof(ModelVertexConstants), alignedModelVertex)
			|| !Align(sizeof(EdgeVertexConstants), alignedEdgeVertex)
			|| !Align(sizeof(GroundShadowVertexConstants), alignedGroundShadowVertex)
			|| !Align(sizeof(ModelPixelConstants), alignedModelPixel)
			|| !Align(sizeof(EdgePixelConstants), alignedEdgePixel)
			|| !Align(sizeof(GroundShadowPixelConstants), alignedGroundShadowPixel)
			|| !BufferSize::TryAdd(drawCount, ringSlack, drawCountWithSlack)
			|| !BufferSize::TryMultiply(drawCount, 2, doubledDrawCountWithSlack)
			|| !BufferSize::TryAdd(doubledDrawCountWithSlack, ringSlack, doubledDrawCountWithSlack)
			|| !AddCapacity(alignedModelVertex, ringSlack, vertexFrameCapacity)
			|| !AddCapacity(alignedEdgeVertex, drawCountWithSlack, vertexFrameCapacity)
			|| !AddCapacity(alignedGroundShadowVertex, ringSlack, vertexFrameCapacity)
			|| !AddCapacity(alignedModelPixel, doubledDrawCountWithSlack, pixelFrameCapacity)
			|| !AddCapacity(alignedEdgePixel, drawCountWithSlack, pixelFrameCapacity)
			|| !AddCapacity(alignedGroundShadowPixel, ringSlack, pixelFrameCapacity)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan uniform buffer ring 크기 계산",
				"material 수에 따른 uniform buffer ring 크기가 한도를 넘습니다"));
		}
		size_t vertexCapacity = 0;
		size_t pixelCapacity = 0;
		if (!BufferSize::TryMultiply(
			vertexFrameCapacity, FrameBuffering::vulkanFramesInFlight, vertexCapacity)
			|| !BufferSize::TryMultiply(
				pixelFrameCapacity, FrameBuffering::vulkanFramesInFlight, pixelCapacity)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan uniform buffer ring 크기 계산",
				"프레임 수에 따른 uniform buffer ring 크기가 한도를 넘습니다"));
		}
		auto result = modelResources.vertexConstantsRing.Setup(
			device, vertexCapacity);
		if (!result)
			return std::unexpected(result.error());
		result = modelResources.pixelConstantsRing.Setup(
			device, pixelCapacity);
		if (!result)
			return std::unexpected(result.error());
		return {};
	}

	GraphicsError::Result<void> VulkanInstance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			VulkanModelMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.texture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult) {
					material.texture = **textureResult;
					material.textureEnabled = true;
				} else
					material.texture = dummyTexture;
			} else
				material.texture = dummyTexture;
			if (!mat.spTexture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.spTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult) {
					material.sphereTexture = **textureResult;
					material.sphereTextureEnabled = true;
				} else
					material.sphereTexture = dummyTexture;
			} else
				material.sphereTexture = dummyTexture;
			if (!mat.toonTexture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.toonTexture, true);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult) {
					material.toonTexture = **textureResult;
					material.toonTextureEnabled = true;
				} else
					material.toonTexture = dummyTexture;
			} else
				material.toonTexture = dummyTexture;
			modelResources.materials.emplace_back(std::move(material));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanInstance::CreateDescriptorSets() {
		const auto modelResult = modelResources.modelDescriptorSet.Initialize(device, pipeline,
			modelResources.vertexConstantsRing.GetBuffer(), sizeof(ModelVertexConstants),
			modelResources.pixelConstantsRing.GetBuffer(), sizeof(ModelPixelConstants),
			modelResources.materials, true);
		if (!modelResult)
			return std::unexpected(modelResult.error());
		const auto edgeResult = modelResources.edgeDescriptorSet.Initialize(device, pipeline,
			modelResources.vertexConstantsRing.GetBuffer(), sizeof(EdgeVertexConstants),
			modelResources.pixelConstantsRing.GetBuffer(), sizeof(EdgePixelConstants),
			modelResources.materials, false);
		if (!edgeResult)
			return std::unexpected(edgeResult.error());
		const auto groundShadowResult = modelResources.groundShadowDescriptorSet.Initialize(device, pipeline,
			modelResources.vertexConstantsRing.GetBuffer(), sizeof(GroundShadowVertexConstants),
			modelResources.pixelConstantsRing.GetBuffer(), sizeof(GroundShadowPixelConstants),
			modelResources.materials, false);
		if (!groundShadowResult)
			return std::unexpected(groundShadowResult.error());
		return {};
	}

	VulkanInstance::VulkanInstance(const VulkanDevice& sourceDevice,
		const VulkanPipeline& sourcePipeline, VulkanUploadContext& sourceUploadContext,
		VulkanTextureCache& sourceTextureCache, const VulkanTexture& sourceDummyTexture,
		VulkanDrawContext& sourceDrawContext)
		: Instance(GraphicsApi::Vulkan), device(sourceDevice), pipeline(sourcePipeline),
		uploadContext(sourceUploadContext),
		textureCache(sourceTextureCache), dummyTexture(sourceDummyTexture),
		drawContext(sourceDrawContext) {
		drawer = std::make_unique<VulkanDrawer>(*this, modelResources, drawContext);
	}

	void VulkanInstance::ResetRendererResources() {
		for (auto& vertexBuffer : modelResources.vertexBuffers)
			vertexBuffer.Reset();
		modelResources.indexBuffer.Reset();
		modelResources.vertexConstantsRing.Clear();
		modelResources.pixelConstantsRing.Clear();
		modelResources.modelDescriptorSet.Reset();
		modelResources.edgeDescriptorSet.Reset();
		modelResources.groundShadowDescriptorSet.Reset();
		modelResources.materials.clear();
		modelResources.uniformBufferOffsetAlignment = 1;
		modelResources.indexType = VK_INDEX_TYPE_UINT16;
	}

	GraphicsError::Result<void> VulkanInstance::SetupRenderer() {
		const auto beginUploadResult = textureCache.BeginUploadBatch(device);
		if (!beginUploadResult)
			return std::unexpected(beginUploadResult.error());
		const auto geometryResult = CreateGeometryBuffers();
		if (!geometryResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(geometryResult.error());
		}
		const auto constantRingResult = SetupConstantRings();
		if (!constantRingResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(constantRingResult.error());
		}
		const auto materialResult = LoadMaterials();
		if (!materialResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(materialResult.error());
		}
		const auto uploadResult = textureCache.SubmitUploadBatch(device);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		return CreateDescriptorSets();
	}

	GraphicsError::Result<void> VulkanInstance::UploadCore() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = modelResources.vertexBuffers[
			frameIndex % FrameBuffering::vulkanFramesInFlight];
		if (vertexBuffer.GetBuffer() == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"Vulkan 모델 정점 업로드", "현재 프레임의 vertex buffer가 초기화되지 않았습니다"));
		const size_t vertexCount = model->geometryData.positions.size();
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount });
		if (!writeSucceeded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 모델 정점 업로드", "vertex 데이터를 기록하지 못했습니다"));
		return {};
	}
}
