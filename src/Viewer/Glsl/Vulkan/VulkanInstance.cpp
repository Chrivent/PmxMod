#include "VulkanInstance.h"

#include "VulkanDrawer.h"
#include "VulkanViewer.h"
#include "../GlslShaderConstants.h"
#include "../../../Model/Model.h"
#include "../../../Model/ModelPose.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace Chrivent {
	bool VulkanInstance::CreateGeometryBuffers(VulkanInstanceInfo& info, const VulkanDeviceInfo& deviceInfo) {
		const auto& geometryData = info.model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			std::cerr << "Failed to build Vulkan index data.\n";
			return false;
		}
		info.indexType = indexData.elementSize == sizeof(uint16_t)
			? VK_INDEX_TYPE_UINT16
			: VK_INDEX_TYPE_UINT32;
		const std::vector<VulkanVertex> vertices = ViewerGeometry::BuildVertices(geometryData, false);
		const VkDeviceSize vertexBufferSize = sizeof(VulkanVertex) * vertices.size();
		const VkDeviceSize indexBufferSize = indexData.bytes.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			std::cerr << "Failed to create Vulkan model buffers: model has no geometry data.\n";
			return false;
		}
		for (auto& vertexBuffer : info.vertexBuffers) {
			if (!vertexBuffer.Initialize(
				deviceInfo,
				vertexBufferSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			if (!vertexBuffer.Write(vertices.data(), vertexBufferSize))
				return false;
		}
		if (!info.indexBuffer.Initialize(
			deviceInfo,
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!info.indexBuffer.Write(indexData.bytes.data(), indexBufferSize))
			return false;
		info.indexCount = indexData.indexCount;
		return true;
	}

	bool VulkanInstance::SetupConstantRings(VulkanInstanceInfo& info, const VulkanDeviceInfo& deviceInfo) {
		info.uniformBufferOffsetAlignment = std::max<size_t>(1, deviceInfo.properties.limits.minUniformBufferOffsetAlignment);
		const size_t drawCount = std::max<size_t>(1, info.model->materialData.subMeshes.size());
		constexpr size_t ringSlack = 2;
		const auto AlignedSize = [&](const size_t size) {
			const size_t alignment = info.uniformBufferOffsetAlignment;
			const size_t remainder = size % alignment;
			if (remainder == 0)
				return size;
			return size + (alignment - remainder);
		};
		constexpr size_t bufferedFrames = 2;
		std::string error;
		if (!info.modelVertexConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(ModelVertexConstants)) * ringSlack * bufferedFrames, error))
			return false;
		if (!info.edgeVertexConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(EdgeVertexConstants)) * (drawCount + ringSlack) * bufferedFrames, error))
			return false;
		if (!info.groundShadowVertexConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(GroundShadowVertexConstants)) * ringSlack * bufferedFrames, error))
			return false;
		if (!info.modelPixelConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(ModelPixelConstants)) * (drawCount + ringSlack) * bufferedFrames, error))
			return false;
		if (!info.edgePixelConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(EdgePixelConstants)) * (drawCount + ringSlack) * bufferedFrames, error))
			return false;
		if (!info.groundShadowPixelConstantsRing.Setup(deviceInfo, AlignedSize(sizeof(GroundShadowPixelConstants)) * (drawCount + ringSlack) * bufferedFrames, error))
			return false;
		return true;
	}

	void VulkanInstance::LoadMaterials(VulkanInstanceInfo& info, const VulkanTexture& dummyTexture) {
		for (const auto& mat : info.model->materialData.materials) {
			VulkanMaterial material(mat);
			if (!mat.texture.empty()) {
				material.texture = info.viewer->LoadTexture(mat.texture);
				if (material.texture.image == VK_NULL_HANDLE)
					material.texture = dummyTexture;
				else
					material.textureEnabled = true;
			} else
				material.texture = dummyTexture;
			if (!mat.spTexture.empty()) {
				material.sphereTexture = info.viewer->LoadTexture(mat.spTexture);
				if (material.sphereTexture.image == VK_NULL_HANDLE)
					material.sphereTexture = dummyTexture;
				else
					material.sphereTextureEnabled = true;
			} else
				material.sphereTexture = dummyTexture;
			if (!mat.toonTexture.empty()) {
				material.toonTexture = info.viewer->LoadTexture(mat.toonTexture, true);
				if (material.toonTexture.image == VK_NULL_HANDLE)
					material.toonTexture = dummyTexture;
				else
					material.toonTextureEnabled = true;
			} else
				material.toonTexture = dummyTexture;
			info.materials.emplace_back(std::move(material));
		}
	}

	bool VulkanInstance::CreateDescriptorSets(
		VulkanInstanceInfo& info,
		const VulkanDeviceInfo& deviceInfo,
		const VulkanPipelineInfo& pipelineInfo) {
		if (!info.modelDescriptorSet.Initialize(
			deviceInfo,
			pipelineInfo,
			info.modelVertexConstantsRing.GetBuffer().GetInfo(),
			sizeof(ModelVertexConstants),
			info.modelPixelConstantsRing.GetBuffer().GetInfo(),
			sizeof(ModelPixelConstants),
			info.materials,
			VulkanPassType::Model))
			return false;
		if (!info.edgeDescriptorSet.Initialize(
			deviceInfo,
			pipelineInfo,
			info.edgeVertexConstantsRing.GetBuffer().GetInfo(),
			sizeof(EdgeVertexConstants),
			info.edgePixelConstantsRing.GetBuffer().GetInfo(),
			sizeof(EdgePixelConstants),
			info.materials,
			VulkanPassType::Edge))
			return false;
		if (!info.groundShadowDescriptorSet.Initialize(
			deviceInfo,
			pipelineInfo,
			info.groundShadowVertexConstantsRing.GetBuffer().GetInfo(),
			sizeof(GroundShadowVertexConstants),
			info.groundShadowPixelConstantsRing.GetBuffer().GetInfo(),
			sizeof(GroundShadowPixelConstants),
			info.materials,
			VulkanPassType::GroundShadow))
			return false;
		return true;
	}

	VulkanInstance::VulkanInstance() {
		info = std::make_unique<VulkanInstanceInfo>();
		drawer = std::make_unique<VulkanDrawer>(static_cast<VulkanInstanceInfo&>(GetInfo()));
	}

	void VulkanInstance::Clear() {
		auto& info = static_cast<VulkanInstanceInfo&>(GetInfo());
		for (auto& vertexBuffer : info.vertexBuffers)
			vertexBuffer.Destroy();
		info.indexBuffer.Destroy();
		info.modelVertexConstantsRing.Clear();
		info.edgeVertexConstantsRing.Clear();
		info.groundShadowVertexConstantsRing.Clear();
		info.modelPixelConstantsRing.Clear();
		info.edgePixelConstantsRing.Clear();
		info.groundShadowPixelConstantsRing.Clear();
		info.modelDescriptorSet.Destroy();
		info.edgeDescriptorSet.Destroy();
		info.groundShadowDescriptorSet.Destroy();
		info.materials.clear();
		info.uniformBufferOffsetAlignment = 1;
		info.indexType = VK_INDEX_TYPE_UINT16;
		info.indexCount = 0;
	}

	bool VulkanInstance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<VulkanInstanceInfo&>(GetInfo());
		Clear();
		info.viewer = static_cast<VulkanViewer*>(&baseViewer);
		if (info.model == nullptr)
			return false;
		const auto& viewerInfo = info.viewer->GetVulkanInfo();
		if (!viewerInfo.deviceInfo ||
			!viewerInfo.pipelineInfo ||
			!viewerInfo.dummyTexture)
			return false;
		const auto& deviceInfo = *viewerInfo.deviceInfo;
		const auto& pipelineInfo = *viewerInfo.pipelineInfo;
		const auto& dummyTexture = *viewerInfo.dummyTexture;
		if (!CreateGeometryBuffers(info, deviceInfo))
			return false;
		if (!SetupConstantRings(info, deviceInfo))
			return false;
		LoadMaterials(info, dummyTexture);
		return CreateDescriptorSets(info, deviceInfo, pipelineInfo);
	}

	void VulkanInstance::Update() const {
		const auto& info = static_cast<const VulkanInstanceInfo&>(GetInfo());
		if (info.model == nullptr || info.viewer == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetVulkanInfo();
		if (!viewerInfo.syncInfo)
			return;
		const size_t frameIndex = viewerInfo.syncInfo->currentFrame % VulkanInstanceInfo::kBufferedFrames;
		const auto& vertexBuffer = info.vertexBuffers[frameIndex];
		if (vertexBuffer.GetInfo().buffer == VK_NULL_HANDLE)
			return;
		const ModelPose pose(*info.model);
		pose.Update();
		const std::vector<VulkanVertex> vertices = ViewerGeometry::BuildVertices(info.model->geometryData, true);
		const VkDeviceSize vertexBufferSize = sizeof(VulkanVertex) * vertices.size();
		if (!vertexBuffer.Write(vertices.data(), vertexBufferSize))
			std::cerr << "Failed to update Vulkan vertex buffer.\n";
	}
}
