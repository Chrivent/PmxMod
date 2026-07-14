#pragma once

#include <d3d12.h>

namespace Chrivent {
	// D3D12 리소스 상태 전환을 Enhanced Barrier와 기존 barrier로 추상화한다.
	class Dx12Barrier {
		// 기존 resource state에 대응하는 Enhanced Barrier 동기화 범위를 반환한다.
		static D3D12_BARRIER_SYNC ResolveSync(D3D12_RESOURCE_STATES state);
		// 기존 resource state에 대응하는 Enhanced Barrier 접근 범위를 반환한다.
		static D3D12_BARRIER_ACCESS ResolveAccess(D3D12_RESOURCE_STATES state);
		// 기존 resource state에 대응하는 Enhanced Barrier 레이아웃을 반환한다.
		static D3D12_BARRIER_LAYOUT ResolveLayout(D3D12_RESOURCE_STATES state);

	public:
		// 지원 장치에서는 Enhanced Barrier로, 그 외에는 기존 barrier로 텍스처 상태를 전환한다.
		static void Transition(ID3D12GraphicsCommandList* commandList,
			ID3D12GraphicsCommandList7* enhancedCommandList, ID3D12Resource* resource,
			D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	};
}
