#pragma once

namespace Chrivent {
	class Dx12CommandContext {
	public:
		// DX12 명령 큐와 프레임 명령 기록 리소스를 초기화한다.
		bool Initialize();
		// 생성한 DX12 명령 리소스를 해제한다.
		void Destroy();
	};
}
