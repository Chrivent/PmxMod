#include "Viewer/Instance/VulkanInstance.h"

#include "Viewer/Drawer/VulkanDrawer.h"
#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Texture/VulkanTextureCache.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/Model.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace Chrivent {
	bool VulkanInstance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			std::cerr << "Failed to build Vulkan index data.\n";
			return false;
		}
		modelResources.indexType = indexData.elementSize == sizeof(uint16_t)
			? VK_INDEX_TYPE_UINT16
			: VK_INDEX_TYPE_UINT32;
		const size_t vertexCount = geometryData.positions.size();
		const VkDeviceSize vertexBufferSize = sizeof(ViewerVertex) * vertexCount;
		const VkDeviceSize indexBufferSize = indexData.bytes.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			std::cerr << "Failed to create Vulkan model buffers: model has no geometry data.\n";
			return false;
		}
		for (auto& vertexBuffer : modelResources.vertexBuffers) {
			if (!vertexBuffer.Initialize(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			if (!ViewerGeometry::WriteVertices(geometryData, false,
				{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount }))
				return false;
		}
		if (!modelResources.indexBuffer.Initialize(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!modelResources.indexBuffer.Write(indexData.bytes.data(), indexBufferSize))
			return false;
		return true;
	}

	bool VulkanInstance::SetupConstantRings() {
		modelResources.uniformBufferOffsetAlignment = std::max<size_t>(
			1, device.properties.limits.minUniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		std::string error;
		if (!modelResources.modelVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		if (!modelResources.edgeVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgeVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		if (!modelResources.groundShadowVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		if (!modelResources.modelPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount * 2 + ringSlack) * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		if (!modelResources.edgePixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgePixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		if (!modelResources.groundShadowPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * FrameBuffering::vulkanFramesInFlight, error))
			return false;
		return true;
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

	bool VulkanInstance::CreateDescriptorSets() {
		if (!modelResources.modelDescriptorSet.Initialize(device, pipeline,
			modelResources.modelVertexConstantsRing.GetBuffer(), sizeof(ModelVertexConstants),
			modelResources.modelPixelConstantsRing.GetBuffer(), sizeof(ModelPixelConstants),
			modelResources.materials, VulkanPassType::Model))
			return false;
		if (!modelResources.edgeDescriptorSet.Initialize(device, pipeline,
			modelResources.edgeVertexConstantsRing.GetBuffer(), sizeof(EdgeVertexConstants),
			modelResources.edgePixelConstantsRing.GetBuffer(), sizeof(EdgePixelConstants),
			modelResources.materials, VulkanPassType::Edge))
			return false;
		if (!modelResources.groundShadowDescriptorSet.Initialize(device, pipeline,
			modelResources.groundShadowVertexConstantsRing.GetBuffer(), sizeof(GroundShadowVertexConstants),
			modelResources.groundShadowPixelConstantsRing.GetBuffer(), sizeof(GroundShadowPixelConstants),
			modelResources.materials, VulkanPassType::GroundShadow))
			return false;
		return true;
	}

	VulkanInstance::VulkanInstance(Viewer& sourceViewer, const VulkanDevice& sourceDevice,
		const VulkanPipeline& sourcePipeline, VulkanTextureCache& sourceTextureCache,
		const VulkanTexture& sourceDummyTexture, VulkanDrawContext& sourceDrawContext)
		: device(sourceDevice), pipeline(sourcePipeline), textureCache(sourceTextureCache),
		dummyTexture(sourceDummyTexture), drawContext(sourceDrawContext) {
		drawer = std::make_unique<VulkanDrawer>(*this, modelResources, drawContext, sourceViewer);
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

	bool VulkanInstance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return false;
		if (!SetupConstantRings())
			return false;
		LoadMaterials();
		return CreateDescriptorSets();
	}

	bool VulkanInstance::Upload() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = modelResources.vertexBuffers[
			frameIndex % FrameBuffering::vulkanFramesInFlight];
		if (vertexBuffer.buffer == VK_NULL_HANDLE)
			return false;
		const size_t vertexCount = model->geometryData.positions.size();
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount });
		if (!writeSucceeded)
			std::cerr << "Failed to update Vulkan vertex buffer.\n";
		return writeSucceeded;
	}
}
