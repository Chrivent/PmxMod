#include "Viewer/Dx12/Helper/Dx12PostProcessTarget.h"

namespace Chrivent {
	bool Dx12PostProcessTarget::Initialize(const Dx12Device& sourceDevice, const int width, const int height) {
		Destroy();
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc.Count = 1;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&sceneColor))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap))))
			return false;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		sourceDevice.device->CreateShaderResourceView(
			sceneColor.Get(), &srvDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Dx12PostProcessTarget::ResolveGpuHandle() const {
		if (!descriptorHeap)
			return {};
		return descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	void Dx12PostProcessTarget::Destroy() {
		descriptorHeap.Reset();
		sceneColor.Reset();
	}
}
