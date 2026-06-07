#pragma once

namespace Chrivent {
	class Dx12Device {
	public:
		// DX12 디바이스와 command queue를 생성한다.
		bool Initialize();
		// 생성한 DX12 디바이스 리소스를 해제한다.
		void Destroy();
	};
}
