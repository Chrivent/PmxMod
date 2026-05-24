#include "VulkanInstance.h"

#include "VulkanDrawer.h"
#include "VulkanViewer.h"
#include "../../Model/Model.h"
#include "../../Model/ModelPose.h"

#include <iostream>

namespace Chrivent {
	VulkanInstance::VulkanInstance() {
		info = std::make_unique<VulkanInstanceInfo>();
		drawer = std::make_unique<VulkanDrawer>(static_cast<VulkanInstanceInfo&>(GetInfo()));
	}

	void VulkanInstance::Clear() {
		auto& info = static_cast<VulkanInstanceInfo&>(GetInfo());
		info.vertexBuffer.Destroy();
		info.indexBuffer.Destroy();
		info.materials.clear();
		info.indexType = VK_INDEX_TYPE_UINT16;
		info.indexCount = 0;
	}

	bool VulkanInstance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<VulkanInstanceInfo&>(GetInfo());
		Clear();
		info.viewer = dynamic_cast<VulkanViewer*>(&baseViewer);
		if (info.viewer == nullptr || info.model == nullptr)
			return false;

		const auto& geometryData = info.model->geometryData;
		if (!GetIndexType(geometryData.indexElementSize, info.indexType)) {
			std::cerr << "Failed to get Vulkan index type.\n";
			return false;
		}
		const std::vector<VulkanVertex> vertices = MakeVertices(geometryData, false);
		const VkDeviceSize vertexBufferSize = sizeof(VulkanVertex) * vertices.size();
		const VkDeviceSize indexBufferSize = geometryData.indices.size();
		if (vertexBufferSize == 0 || indexBufferSize == 0) {
			std::cerr << "Failed to create Vulkan model buffers: model has no geometry data.\n";
			return false;
		}

		const auto& deviceInfo = info.viewer->GetDeviceInfo();
		if (!info.vertexBuffer.Initialize(
			deviceInfo,
			vertexBufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!info.vertexBuffer.Write(vertices.data(), vertexBufferSize))
			return false;
		if (!info.indexBuffer.Initialize(
			deviceInfo,
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!info.indexBuffer.Write(geometryData.indices.data(), indexBufferSize))
			return false;
		info.indexCount = geometryData.indexCount;

		for (const auto& mat : info.model->materialData.materials) {
			VulkanMaterial material(mat);
			if (!mat.texture.empty())
				material.texture = info.viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = info.viewer->LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = info.viewer->LoadTexture(mat.toonTexture);
			info.materials.emplace_back(std::move(material));
		}
		return true;
	}

	void VulkanInstance::Update() const {
		const auto& info = static_cast<const VulkanInstanceInfo&>(GetInfo());
		if (info.model == nullptr || info.vertexBuffer.GetInfo().buffer == VK_NULL_HANDLE)
			return;
		const ModelPose pose(*info.model);
		pose.Update();
		const std::vector<VulkanVertex> vertices = MakeVertices(info.model->geometryData, true);
		const VkDeviceSize vertexBufferSize = sizeof(VulkanVertex) * vertices.size();
		if (!info.vertexBuffer.Write(vertices.data(), vertexBufferSize))
			std::cerr << "Failed to update Vulkan vertex buffer.\n";
	}

	std::vector<VulkanVertex> VulkanInstance::MakeVertices(const ModelGeometryData& geometryData, const bool useUpdateData) {
		const auto& positions = useUpdateData && geometryData.updatePositions.size() == geometryData.positions.size()
			? geometryData.updatePositions
			: geometryData.positions;
		const auto& normals = useUpdateData && geometryData.updateNormals.size() == geometryData.normals.size()
			? geometryData.updateNormals
			: geometryData.normals;
		const auto& uvs = useUpdateData && geometryData.updateUVs.size() == geometryData.uvs.size()
			? geometryData.updateUVs
			: geometryData.uvs;

		std::vector<VulkanVertex> vertices;
		vertices.reserve(positions.size());
		for (size_t i = 0; i < positions.size(); i++) {
			VulkanVertex vertex{};
			vertex.position = positions[i];
			if (i < normals.size())
				vertex.normal = normals[i];
			if (i < uvs.size())
				vertex.uv = uvs[i];
			vertices.emplace_back(vertex);
		}
		return vertices;
	}

	bool VulkanInstance::GetIndexType(const size_t indexElementSize, VkIndexType& indexType) {
		if (indexElementSize == 2) {
			indexType = VK_INDEX_TYPE_UINT16;
			return true;
		}
		if (indexElementSize == 4) {
			indexType = VK_INDEX_TYPE_UINT32;
			return true;
		}
		return false;
	}
}
