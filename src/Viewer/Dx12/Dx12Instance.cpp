#include "Dx12Instance.h"

#include "Dx12Drawer.h"
#include "Dx12Viewer.h"
#include "../../Model/Model.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	std::vector<Dx12Vertex> Dx12Instance::BuildVertices(const ModelGeometryData& geometryData, const bool useUpdateData) {
		const auto& positions = useUpdateData && geometryData.updatePositions.size() == geometryData.positions.size()
			? geometryData.updatePositions
			: geometryData.positions;
		const auto& normals = useUpdateData && geometryData.updateNormals.size() == geometryData.normals.size()
			? geometryData.updateNormals
			: geometryData.normals;
		const auto& uvs = useUpdateData && geometryData.updateUVs.size() == geometryData.uvs.size()
			? geometryData.updateUVs
			: geometryData.uvs;
		std::vector<Dx12Vertex> vertices(positions.size());
		for (size_t index = 0; index < positions.size(); index++) {
			auto& [position, normal, uv] = vertices[index];
			position = positions[index];
			if (index < normals.size())
				normal = normals[index];
			if (index < uvs.size())
				uv = uvs[index];
		}
		return vertices;
	}

	bool Dx12Instance::BuildIndexData(const ModelGeometryData& geometryData, std::vector<char>& indices, DXGI_FORMAT& format) {
		indices.clear();
		if (geometryData.indexElementSize == 1) {
			std::vector<uint16_t> convertedIndices(geometryData.indexCount);
			for (size_t index = 0; index < geometryData.indexCount; index++)
				convertedIndices[index] = geometryData.indices[index];
			const size_t byteSize = sizeof(uint16_t) * convertedIndices.size();
			const auto* bytes = reinterpret_cast<const char*>(convertedIndices.data());
			indices.assign(bytes, bytes + byteSize);
			format = DXGI_FORMAT_R16_UINT;
			return true;
		}
		if (geometryData.indexElementSize == 2) {
			indices = geometryData.indices;
			format = DXGI_FORMAT_R16_UINT;
			return true;
		}
		if (geometryData.indexElementSize == 4) {
			indices = geometryData.indices;
			format = DXGI_FORMAT_R32_UINT;
			return true;
		}
		return false;
	}

	Dx12Instance::Dx12Instance() {
		info = std::make_unique<Dx12InstanceInfo>();
		drawer = std::make_unique<Dx12Drawer>(static_cast<Dx12InstanceInfo&>(GetInfo()));
	}

	void Dx12Instance::Clear() {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		info.vertexBuffer.Destroy();
		info.indexBuffer.Destroy();
		info.vertexBufferView = {};
		info.indexBufferView = {};
		info.indexCount = 0;
		info.materials.clear();
	}

	bool Dx12Instance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		Clear();
		info.viewer = static_cast<Dx12Viewer*>(&baseViewer);
		if (info.model == nullptr)
			return false;
		const auto& geometryData = info.model->geometryData;
		std::vector<Dx12Vertex> vertices = BuildVertices(geometryData, false);
		std::vector<char> indices;
		DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
		if (vertices.empty() || !BuildIndexData(geometryData, indices, indexFormat) || indices.empty()) {
			std::cerr << "Failed to create DX12 model buffers: model has no geometry data.\n";
			return false;
		}
		const size_t vertexByteSize = sizeof(Dx12Vertex) * vertices.size();
		if (vertexByteSize > (std::numeric_limits<UINT>::max)() || indices.size() > (std::numeric_limits<UINT>::max)())
			return false;
		const auto& deviceInfo = info.viewer->GetDeviceInfo();
		if (!info.vertexBuffer.InitializeUpload(deviceInfo, vertexByteSize) ||
			!info.vertexBuffer.Write(vertices.data(), vertexByteSize))
			return false;
		if (!info.indexBuffer.InitializeUpload(deviceInfo, indices.size()) ||
			!info.indexBuffer.Write(indices.data(), indices.size()))
			return false;
		info.vertexBufferView.BufferLocation = info.vertexBuffer.GetGpuAddress();
		info.vertexBufferView.SizeInBytes = vertexByteSize;
		info.vertexBufferView.StrideInBytes = sizeof(Dx12Vertex);
		info.indexBufferView.BufferLocation = info.indexBuffer.GetGpuAddress();
		info.indexBufferView.SizeInBytes = indices.size();
		info.indexBufferView.Format = indexFormat;
		info.indexCount = geometryData.indexCount;
		info.materials.reserve(info.model->materialData.materials.size());
		for (const auto& mat : info.model->materialData.materials)
			info.materials.emplace_back(mat);
		return true;
	}

	void Dx12Instance::Update() const {
		const auto& info = static_cast<const Dx12InstanceInfo&>(GetInfo());
		if (info.model == nullptr || info.vertexBuffer.GetResource() == nullptr)
			return;
		const std::vector<Dx12Vertex> vertices = BuildVertices(info.model->geometryData, true);
		if (vertices.empty())
			return;
		if (!info.vertexBuffer.Write(vertices.data(), sizeof(Dx12Vertex) * vertices.size()))
			std::cerr << "Failed to update DX12 vertex buffer.\n";
	}
}
