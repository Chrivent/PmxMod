#include "Viewer/Instance/VulkanInstance.h"

#include "Viewer/Drawer/VulkanDrawer.h"
#include "Viewer/Viewer/VulkanViewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace Chrivent {
	bool VulkanInstance::CreateGeometryBuffers(const VulkanDevice& device) {
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
				{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount }))
				return false;
		}
		if (!modelResources.indexBuffer.Initialize(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!modelResources.indexBuffer.Write(indexData.bytes.data(), indexBufferSize))
			return false;
		modelResources.indexCount = indexData.indexCount;
		return true;
	}

	bool VulkanInstance::SetupConstantRings(const VulkanDevice& device) {
		modelResources.uniformBufferOffsetAlignment = std::max<size_t>(
			1, device.properties.limits.minUniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		std::string error;
		if (!modelResources.modelVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * VulkanModelResources::kBufferedFrames, error))
			return false;
		if (!modelResources.edgeVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgeVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * VulkanModelResources::kBufferedFrames, error))
			return false;
		if (!modelResources.groundShadowVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowVertexConstants), modelResources.uniformBufferOffsetAlignment)
				* ringSlack * VulkanModelResources::kBufferedFrames, error))
			return false;
		if (!modelResources.modelPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount * 2 + ringSlack) * VulkanModelResources::kBufferedFrames, error))
			return false;
		if (!modelResources.edgePixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgePixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * VulkanModelResources::kBufferedFrames, error))
			return false;
		if (!modelResources.groundShadowPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(
				sizeof(GroundShadowPixelConstants), modelResources.uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * VulkanModelResources::kBufferedFrames, error))
			return false;
		return true;
	}

	void VulkanInstance::LoadMaterials(const VulkanTexture& dummyTexture) {
		for (const auto& mat : model->materialData.materials) {
			VulkanModelMaterial material(mat);
			if (!mat.texture.empty()) {
				material.texture = viewer.LoadTexture(mat.texture);
				if (material.texture.image == VK_NULL_HANDLE)
					material.texture = dummyTexture;
				else
					material.textureEnabled = true;
			} else
				material.texture = dummyTexture;
			if (!mat.spTexture.empty()) {
				material.sphereTexture = viewer.LoadTexture(mat.spTexture);
				if (material.sphereTexture.image == VK_NULL_HANDLE)
					material.sphereTexture = dummyTexture;
				else
					material.sphereTextureEnabled = true;
			} else
				material.sphereTexture = dummyTexture;
			if (!mat.toonTexture.empty()) {
				material.toonTexture = viewer.LoadTexture(mat.toonTexture, true);
				if (material.toonTexture.image == VK_NULL_HANDLE)
					material.toonTexture = dummyTexture;
				else
					material.toonTextureEnabled = true;
			} else
				material.toonTexture = dummyTexture;
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	bool VulkanInstance::CreateDescriptorSets(const VulkanDevice& device, const VulkanPipeline& pipeline) {
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

	VulkanInstance::VulkanInstance(VulkanViewer& sourceViewer) : viewer(sourceViewer) {
		drawer = std::make_unique<VulkanDrawer>(*this, modelResources, viewer.GetDrawContext(), viewer);
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
		modelResources.indexCount = 0;
	}

	bool VulkanInstance::SetupRenderer() {
		const VulkanDevice& device = viewer.GetDevice();
		const VulkanPipeline& pipeline = viewer.GetPipeline();
		const VulkanTexture& dummyTexture = viewer.GetDummyTexture();
		if (!CreateGeometryBuffers(device))
			return false;
		if (!SetupConstantRings(device))
			return false;
		LoadMaterials(dummyTexture);
		return CreateDescriptorSets(device, pipeline);
	}

	bool VulkanInstance::Upload() {
		if (model == nullptr)
			return false;
		const size_t frameIndex = viewer.GetFrameIndex();
		const auto& vertexBuffer = modelResources.vertexBuffers[
			frameIndex % VulkanModelResources::kBufferedFrames];
		if (vertexBuffer.buffer == VK_NULL_HANDLE)
			return false;
		const size_t vertexCount = model->geometryData.positions.size();
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount });
		if (!writeSucceeded)
			std::cerr << "Failed to update Vulkan vertex buffer.\n";
		return writeSucceeded;
	}
}
