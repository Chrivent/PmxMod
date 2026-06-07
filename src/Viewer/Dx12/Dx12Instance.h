#pragma once

#include "../Instance.h"

#include <vector>

namespace Chrivent {
	class Dx12Drawer;
	class Dx12Viewer;
	struct Dx12Material;

	struct Dx12InstanceInfo : InstanceInfo {
		Dx12Viewer* viewer = nullptr;
		std::vector<Dx12Material> materials;
	};

	class Dx12Instance : public Instance {
	public:
		Dx12Instance();
		~Dx12Instance() override = default;

		// DX12 모델 리소스를 해제한다.
		void Clear() override;
		// 모델 데이터를 DX12 리소스로 업로드한다.
		bool Setup(Viewer& baseViewer) override;
		// 모델의 갱신된 버텍스 데이터를 DX12 리소스에 반영한다.
		void Update() const override;
	};
}
