#pragma once

namespace Chrivent {
	class Dx12SwapChain {
	public:
		// DX12 스왑체인과 back buffer RTV를 생성한다.
		bool Initialize();
		// 창 크기에 맞춰 스왑체인을 다시 생성한다.
		bool Resize();
		// 생성한 DX12 스왑체인 리소스를 해제한다.
		void Destroy();
	};
}
