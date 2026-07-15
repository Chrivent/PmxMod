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
		indexType = indexData.elementSize == sizeof(uint16_t)
			? VK_INDEX_TYPE_UINT16
			: VK_INDEX_TYPE_UINT32;
		const size_t vertexCount = geometryData.positions.size();
		const VkDeviceSize vertexBufferSize = sizeof(ViewerVertex) * vertexCount;
		const VkDeviceSize indexBufferSize = indexData.bytes.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			std::cerr << "Failed to create Vulkan model buffers: model has no geometry data.\n";
			return false;
		}
		for (auto& vertexBuffer : vertexBuffers) {
			if (!vertexBuffer.Initialize(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			if (!ViewerGeometry::WriteVertices(geometryData, false,
				{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount }))
				return false;
		}
		if (!indexBuffer.Initialize(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!indexBuffer.Write(indexData.bytes.data(), indexBufferSize))
			return false;
		indexCount = indexData.indexCount;
		return true;
	}

	bool VulkanInstance::SetupConstantRings(const VulkanDevice& device) {
		uniformBufferOffsetAlignment = std::max<size_t>(1, device.properties.limits.minUniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		std::string error;
		if (!modelVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelVertexConstants), uniformBufferOffsetAlignment)
				* ringSlack * kBufferedFrames, error))
			return false;
		if (!edgeVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgeVertexConstants), uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * kBufferedFrames, error))
			return false;
		if (!groundShadowVertexConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(GroundShadowVertexConstants), uniformBufferOffsetAlignment)
				* ringSlack * kBufferedFrames, error))
			return false;
		if (!modelPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(ModelPixelConstants), uniformBufferOffsetAlignment)
				* (drawCount * 2 + ringSlack) * kBufferedFrames, error))
			return false;
		if (!edgePixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(EdgePixelConstants), uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * kBufferedFrames, error))
			return false;
		if (!groundShadowPixelConstantsRing.Setup(device,
			DynamicBufferRing::AlignUp(sizeof(GroundShadowPixelConstants), uniformBufferOffsetAlignment)
				* (drawCount + ringSlack) * kBufferedFrames, error))
			return false;
		return true;
	}

	void VulkanInstance::LoadMaterials(const VulkanTexture& dummyTexture) {
		for (const auto& mat : model->materialData.materials) {
			VulkanMaterial material(mat);
			if (!mat.texture.empty()) {
				material.texture = viewer->LoadTexture(mat.texture);
				if (material.texture.image == VK_NULL_HANDLE)
					material.texture = dummyTexture;
				else
					material.textureEnabled = true;
			} else
				material.texture = dummyTexture;
			if (!mat.spTexture.empty()) {
				material.sphereTexture = viewer->LoadTexture(mat.spTexture);
				if (material.sphereTexture.image == VK_NULL_HANDLE)
					material.sphereTexture = dummyTexture;
				else
					material.sphereTextureEnabled = true;
			} else
				material.sphereTexture = dummyTexture;
			if (!mat.toonTexture.empty()) {
				material.toonTexture = viewer->LoadTexture(mat.toonTexture, true);
				if (material.toonTexture.image == VK_NULL_HANDLE)
					material.toonTexture = dummyTexture;
				else
					material.toonTextureEnabled = true;
			} else
				material.toonTexture = dummyTexture;
			materials.emplace_back(std::move(material));
		}
	}

	bool VulkanInstance::CreateDescriptorSets(const VulkanDevice& device, const VulkanPipeline& pipeline) {
		if (!modelDescriptorSet.Initialize(device, pipeline,
			modelVertexConstantsRing.GetBuffer(), sizeof(ModelVertexConstants),
			modelPixelConstantsRing.GetBuffer(), sizeof(ModelPixelConstants),
			materials, VulkanPassType::Model))
			return false;
		if (!edgeDescriptorSet.Initialize(device, pipeline,
			edgeVertexConstantsRing.GetBuffer(), sizeof(EdgeVertexConstants),
			edgePixelConstantsRing.GetBuffer(), sizeof(EdgePixelConstants),
			materials, VulkanPassType::Edge))
			return false;
		if (!groundShadowDescriptorSet.Initialize(device, pipeline,
			groundShadowVertexConstantsRing.GetBuffer(), sizeof(GroundShadowVertexConstants),
			groundShadowPixelConstantsRing.GetBuffer(), sizeof(GroundShadowPixelConstants),
			materials, VulkanPassType::GroundShadow))
			return false;
		return true;
	}

	VulkanInstance::VulkanInstance(VulkanViewer& sourceViewer) : viewer(&sourceViewer) {
		drawer = std::make_unique<VulkanDrawer>(*this);
	}

	void VulkanInstance::ResetRendererResources() {
		for (auto& vertexBuffer : vertexBuffers)
			vertexBuffer.Reset();
		indexBuffer.Reset();
		modelVertexConstantsRing.Clear();
		edgeVertexConstantsRing.Clear();
		groundShadowVertexConstantsRing.Clear();
		modelPixelConstantsRing.Clear();
		edgePixelConstantsRing.Clear();
		groundShadowPixelConstantsRing.Clear();
		modelDescriptorSet.Reset();
		edgeDescriptorSet.Reset();
		groundShadowDescriptorSet.Reset();
		materials.clear();
		uniformBufferOffsetAlignment = 1;
		indexType = VK_INDEX_TYPE_UINT16;
		indexCount = 0;
	}

	bool VulkanInstance::SetupRenderer() {
		const auto* devicePointer = viewer->GetDevice();
		const auto* pipelinePointer = viewer->GetPipeline();
		const auto* dummyTexturePointer = viewer->GetDummyTexture();
		if (devicePointer == nullptr || pipelinePointer == nullptr || dummyTexturePointer == nullptr)
			return false;
		const auto& device = *devicePointer;
		const auto& pipeline = *pipelinePointer;
		const auto& dummyTexture = *dummyTexturePointer;
		if (!CreateGeometryBuffers(device))
			return false;
		if (!SetupConstantRings(device))
			return false;
		LoadMaterials(dummyTexture);
		return CreateDescriptorSets(device, pipeline);
	}

	void VulkanInstance::Upload() const {
		if (model == nullptr || viewer == nullptr)
			return;
		size_t frameIndex = 0;
		if (!viewer->ResolveFrameIndex(frameIndex))
			return;
		const auto& vertexBuffer = vertexBuffers[frameIndex % kBufferedFrames];
		if (vertexBuffer.buffer == VK_NULL_HANDLE)
			return;
		const size_t vertexCount = model->geometryData.positions.size();
		if (!ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount }))
			std::cerr << "Failed to update Vulkan vertex buffer.\n";
	}
}
