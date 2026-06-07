#include "Dx12Instance.h"

#include "Dx12Drawer.h"
#include "Dx12Viewer.h"
#include "../../Model/Model.h"

namespace Chrivent {
	Dx12Instance::Dx12Instance() {
		info = std::make_unique<Dx12InstanceInfo>();
		drawer = std::make_unique<Dx12Drawer>(static_cast<Dx12InstanceInfo&>(GetInfo()));
	}

	void Dx12Instance::Clear() {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		info.materials.clear();
	}

	bool Dx12Instance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		Clear();
		info.viewer = static_cast<Dx12Viewer*>(&baseViewer);
		if (info.model == nullptr)
			return false;
		info.materials.reserve(info.model->materialData.materials.size());
		for (const auto& mat : info.model->materialData.materials)
			info.materials.emplace_back(mat);
		return true;
	}

	void Dx12Instance::Update() const {}
}
