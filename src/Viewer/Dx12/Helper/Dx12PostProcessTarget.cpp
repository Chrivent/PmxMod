#include "Viewer/Dx12/Helper/Dx12PostProcessTarget.h"

#include "Viewer/Shader/PostProcessInputLayout.h"

namespace Chrivent {
	bool Dx12PostProcessTarget::Initialize(const Dx12Device& sourceDevice, const int width, const int height) {
		Reset();
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
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&sceneColor))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = PostProcessInputLayout::RequiredTextureCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&srvDescriptorHeap))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap))))
			return false;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		sourceDevice.device->CreateShaderResourceView(
			sceneColor.Get(), &srvDesc, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		UpdateDepthShaderResourceView(sourceDevice, nullptr);
		sourceDevice.device->CreateRenderTargetView(
			sceneColor.Get(), nullptr, rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	void Dx12PostProcessTarget::UpdateDepthShaderResourceView(
		const Dx12Device& sourceDevice, ID3D12Resource* depthResource) const {
		if (!sourceDevice.device || !srvDescriptorHeap)
			return;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += sourceDevice.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
			* PostProcessInputLayout::SceneDepthRegister;
		sourceDevice.device->CreateShaderResourceView(depthResource, &srvDesc, handle);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Dx12PostProcessTarget::ResolveGpuHandle() const {
		if (!srvDescriptorHeap)
			return {};
		return srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12PostProcessTarget::ResolveRtvHandle() const {
		if (!rtvDescriptorHeap)
			return {};
		return rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void Dx12PostProcessTarget::Reset() {
		rtvDescriptorHeap.Reset();
		srvDescriptorHeap.Reset();
		sceneColor.Reset();
	}
}
