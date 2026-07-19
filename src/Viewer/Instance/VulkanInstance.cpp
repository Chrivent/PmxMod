#include "Viewer/Instance/VulkanInstance.h"

#include "Viewer/Drawer/VulkanDrawer.h"
#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Texture/VulkanTextureCache.h"
#include "Core/Model/Model.h"

#include <algorithm>
#include <memory>
#include <string>

namespace Chrivent {
	GraphicsResult<void> VulkanInstance::CreateGeometryBuffers() {
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
		const VkDeviceSize vertexBufferSize = sizeof(ViewerVertex) * vertexCount;
		const VkDeviceSize indexBufferSize = indexData.bytes.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"Vulkan geometry 생성", "vertex 또는 index 데이터가 비어 있습니다"));
		}
		for (auto& vertexBuffer : modelResources.vertexBuffers) {
			if (!vertexBuffer.Initialize(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"Vulkan vertex buffer 생성", "동적 vertex buffer를 만들지 못했습니다"));
			}
			if (!ViewerGeometry::WriteVertices(geometryData, false,
				{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount })) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan vertex buffer 기록", "초기 vertex 데이터를 기록하지 못했습니다"));
			}
		}
		auto indexUploadBuffer = std::make_unique<VulkanBuffer>();
		if (!indexUploadBuffer->Initialize(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan index upload buffer 생성", "index upload buffer를 만들지 못했습니다"));
		}
		if (!indexUploadBuffer->Write(indexData.bytes.data(), indexBufferSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan index upload buffer 기록", "index 데이터를 upload buffer에 기록하지 못했습니다"));
		}
		if (!modelResources.indexBuffer.Initialize(device, indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan index buffer 생성", "GPU index buffer를 만들지 못했습니다"));
		}
		return uploadContext.RecordIndexBufferUpload(
			modelResources.indexBuffer.buffer, std::move(indexUploadBuffer), indexBufferSize);
	}

	GraphicsResult<void> VulkanInstance::SetupConstantRings() {
		modelResources.uniformBufferOffsetAlignment = std::max<size_t>(
			1, device.GetUniformBufferAlignment());
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		const auto align = [this](const size_t size) {
			return DynamicBufferRing::AlignUp(
				size, modelResources.uniformBufferOffsetAlignment);
		};
		const size_t vertexFrameCapacity =
			align(sizeof(ModelVertexConstants)) * ringSlack
			+ align(sizeof(EdgeVertexConstants)) * (drawCount + ringSlack)
			+ align(sizeof(GroundShadowVertexConstants)) * ringSlack;
		const size_t pixelFrameCapacity =
			align(sizeof(ModelPixelConstants)) * (drawCount * 2 + ringSlack)
			+ align(sizeof(EdgePixelConstants)) * (drawCount + ringSlack)
			+ align(sizeof(GroundShadowPixelConstants)) * ringSlack;
		auto result = modelResources.vertexConstantsRing.Setup(
			device, vertexFrameCapacity * FrameBuffering::vulkanFramesInFlight);
		if (!result)
			return std::unexpected(result.error());
		result = modelResources.pixelConstantsRing.Setup(
			device, pixelFrameCapacity * FrameBuffering::vulkanFramesInFlight);
		if (!result)
			return std::unexpected(result.error());
		return {};
	}

	GraphicsResult<void> VulkanInstance::LoadMaterials() {
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

	GraphicsResult<void> VulkanInstance::CreateDescriptorSets() {
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

	GraphicsResult<void> VulkanInstance::SetupRenderer() {
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

	GraphicsResult<void> VulkanInstance::UploadCore() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = modelResources.vertexBuffers[
			frameIndex % FrameBuffering::vulkanFramesInFlight];
		if (vertexBuffer.buffer == VK_NULL_HANDLE)
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
