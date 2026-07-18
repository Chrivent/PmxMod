#include "Viewer/Instance/VulkanInstance.h"

#include "Viewer/Drawer/VulkanDrawer.h"
#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Texture/VulkanTextureCache.h"
#include "Core/Model/Model.h"

#include <algorithm>
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
		VulkanBuffer indexUploadBuffer;
		if (!indexUploadBuffer.Initialize(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan index upload buffer 생성", "index upload buffer를 만들지 못했습니다"));
		}
		if (!indexUploadBuffer.Write(indexData.bytes.data(), indexBufferSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan index upload buffer 기록", "index 데이터를 upload buffer에 기록하지 못했습니다"));
		}
		if (!modelResources.indexBuffer.Initialize(device, indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan index buffer 생성", "GPU index buffer를 만들지 못했습니다"));
		}
		return uploadContext.UploadIndexBuffer(device, modelResources.indexBuffer.buffer,
			indexUploadBuffer.buffer, indexBufferSize);
	}

	GraphicsResult<void> VulkanInstance::SetupConstantRings() {
		modelResources.uniformBufferOffsetAlignment = std::max<size_t>(
			1, device.GetUniformBufferAlignment());
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		std::string error;
		if (!modelResources.modelVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan model vertex constant ring 생성", std::move(error)));
		}
		if (!modelResources.edgeVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgeVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan edge vertex constant ring 생성", std::move(error)));
		}
		if (!modelResources.groundShadowVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan ground shadow vertex constant ring 생성", std::move(error)));
		}
		if (!modelResources.modelPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount * 2 + ringSlack) * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan model pixel constant ring 생성", std::move(error)));
		}
		if (!modelResources.edgePixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgePixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan edge pixel constant ring 생성", std::move(error)));
		}
		if (!modelResources.groundShadowPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * FrameBuffering::vulkanFramesInFlight, error)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"Vulkan ground shadow pixel constant ring 생성", std::move(error)));
		}
		return {};
	}

	void VulkanInstance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			VulkanModelMaterial material(mat);
			if (!mat.texture.empty()) {
				material.texture = textureCache.Load(device, mat.texture);
				if (material.texture.image == VK_NULL_HANDLE)
					material.texture = dummyTexture;
				else
					material.textureEnabled = true;
			} else
				material.texture = dummyTexture;
			if (!mat.spTexture.empty()) {
				material.sphereTexture = textureCache.Load(device, mat.spTexture);
				if (material.sphereTexture.image == VK_NULL_HANDLE)
					material.sphereTexture = dummyTexture;
				else
					material.sphereTextureEnabled = true;
			} else
				material.sphereTexture = dummyTexture;
			if (!mat.toonTexture.empty()) {
				material.toonTexture = textureCache.Load(device, mat.toonTexture, true);
				if (material.toonTexture.image == VK_NULL_HANDLE)
					material.toonTexture = dummyTexture;
				else
					material.toonTextureEnabled = true;
			} else
				material.toonTexture = dummyTexture;
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	GraphicsResult<void> VulkanInstance::CreateDescriptorSets() {
		const auto modelResult = modelResources.modelDescriptorSet.Initialize(device, pipeline,
			modelResources.modelVertexConstantsRing.GetBuffer(), sizeof(ModelVertexConstants),
			modelResources.modelPixelConstantsRing.GetBuffer(), sizeof(ModelPixelConstants),
			modelResources.materials, true);
		if (!modelResult)
			return std::unexpected(modelResult.error());
		const auto edgeResult = modelResources.edgeDescriptorSet.Initialize(device, pipeline,
			modelResources.edgeVertexConstantsRing.GetBuffer(), sizeof(EdgeVertexConstants),
			modelResources.edgePixelConstantsRing.GetBuffer(), sizeof(EdgePixelConstants),
			modelResources.materials, false);
		if (!edgeResult)
			return std::unexpected(edgeResult.error());
		const auto groundShadowResult = modelResources.groundShadowDescriptorSet.Initialize(device, pipeline,
			modelResources.groundShadowVertexConstantsRing.GetBuffer(), sizeof(GroundShadowVertexConstants),
			modelResources.groundShadowPixelConstantsRing.GetBuffer(), sizeof(GroundShadowPixelConstants),
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
		modelResources.modelVertexConstantsRing.Clear();
		modelResources.edgeVertexConstantsRing.Clear();
		modelResources.groundShadowVertexConstantsRing.Clear();
		modelResources.modelPixelConstantsRing.Clear();
		modelResources.edgePixelConstantsRing.Clear();
		modelResources.groundShadowPixelConstantsRing.Clear();
		modelResources.modelDescriptorSet.Reset();
		modelResources.edgeDescriptorSet.Reset();
		modelResources.groundShadowDescriptorSet.Reset();
		modelResources.materials.clear();
		modelResources.uniformBufferOffsetAlignment = 1;
		modelResources.indexType = VK_INDEX_TYPE_UINT16;
	}

	GraphicsResult<void> VulkanInstance::SetupRenderer() {
		const auto geometryResult = CreateGeometryBuffers();
		if (!geometryResult)
			return std::unexpected(geometryResult.error());
		const auto constantRingResult = SetupConstantRings();
		if (!constantRingResult)
			return std::unexpected(constantRingResult.error());
		LoadMaterials();
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
