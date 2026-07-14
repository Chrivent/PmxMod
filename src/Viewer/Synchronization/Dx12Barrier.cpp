#include "Viewer/Synchronization/Dx12Barrier.h"

namespace Chrivent {
	D3D12_BARRIER_SYNC Dx12Barrier::ResolveSync(const D3D12_RESOURCE_STATES state) {
		if (state == D3D12_RESOURCE_STATE_RENDER_TARGET)
			return D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (state == D3D12_RESOURCE_STATE_DEPTH_WRITE)
			return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (state == D3D12_RESOURCE_STATE_RESOLVE_SOURCE || state == D3D12_RESOURCE_STATE_RESOLVE_DEST)
			return D3D12_BARRIER_SYNC_RESOLVE;
		if (state == D3D12_RESOURCE_STATE_COPY_SOURCE || state == D3D12_RESOURCE_STATE_COPY_DEST)
			return D3D12_BARRIER_SYNC_COPY;
		if (state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			return D3D12_BARRIER_SYNC_PIXEL_SHADING;
		return D3D12_BARRIER_SYNC_NONE;
	}

	D3D12_BARRIER_ACCESS Dx12Barrier::ResolveAccess(const D3D12_RESOURCE_STATES state) {
		if (state == D3D12_RESOURCE_STATE_RENDER_TARGET)
			return D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (state == D3D12_RESOURCE_STATE_DEPTH_WRITE)
			return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (state == D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
			return D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
		if (state == D3D12_RESOURCE_STATE_RESOLVE_DEST)
			return D3D12_BARRIER_ACCESS_RESOLVE_DEST;
		if (state == D3D12_RESOURCE_STATE_COPY_SOURCE)
			return D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (state == D3D12_RESOURCE_STATE_COPY_DEST)
			return D3D12_BARRIER_ACCESS_COPY_DEST;
		if (state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		return D3D12_BARRIER_ACCESS_NO_ACCESS;
	}

	D3D12_BARRIER_LAYOUT Dx12Barrier::ResolveLayout(const D3D12_RESOURCE_STATES state) {
		if (state == D3D12_RESOURCE_STATE_RENDER_TARGET)
			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (state == D3D12_RESOURCE_STATE_DEPTH_WRITE)
			return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (state == D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
			return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
		if (state == D3D12_RESOURCE_STATE_RESOLVE_DEST)
			return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
		if (state == D3D12_RESOURCE_STATE_COPY_SOURCE)
			return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (state == D3D12_RESOURCE_STATE_COPY_DEST)
			return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		return D3D12_BARRIER_LAYOUT_PRESENT;
	}

	void Dx12Barrier::Transition(ID3D12GraphicsCommandList* commandList,
		ID3D12GraphicsCommandList7* enhancedCommandList, ID3D12Resource* resource,
		const D3D12_RESOURCE_STATES before, const D3D12_RESOURCE_STATES after) {
		if (enhancedCommandList != nullptr) {
			D3D12_TEXTURE_BARRIER textureBarrier{};
			textureBarrier.SyncBefore = ResolveSync(before);
			textureBarrier.SyncAfter = ResolveSync(after);
			textureBarrier.AccessBefore = ResolveAccess(before);
			textureBarrier.AccessAfter = ResolveAccess(after);
			textureBarrier.LayoutBefore = ResolveLayout(before);
			textureBarrier.LayoutAfter = ResolveLayout(after);
			textureBarrier.pResource = resource;
			textureBarrier.Subresources.IndexOrFirstMipLevel = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			D3D12_BARRIER_GROUP barrierGroup;
			barrierGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
			barrierGroup.NumBarriers = 1;
			barrierGroup.pTextureBarriers = &textureBarrier;
			enhancedCommandList->Barrier(1, &barrierGroup);
			return;
		}
		D3D12_RESOURCE_BARRIER resourceBarrier{};
		resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		resourceBarrier.Transition.pResource = resource;
		resourceBarrier.Transition.StateBefore = before;
		resourceBarrier.Transition.StateAfter = after;
		resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &resourceBarrier);
	}
}
