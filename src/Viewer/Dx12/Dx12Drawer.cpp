#include "Dx12Drawer.h"

#include "Dx12Instance.h"
#include "Dx12Viewer.h"

namespace Chrivent {
	void Dx12Drawer::DrawModel() {
		if (info.viewer == nullptr || info.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = info.viewer->GetCommandList();
		if (commandList == nullptr)
			return;
		commandList->IASetVertexBuffers(0, 1, &info.vertexBufferView);
		commandList->IASetIndexBuffer(&info.indexBufferView);
		commandList->DrawIndexedInstanced(info.indexCount, 1, 0, 0, 0);
	}

	void Dx12Drawer::DrawEdge() {}

	void Dx12Drawer::DrawGroundShadow() {}

	Dx12Drawer::Dx12Drawer(const Dx12InstanceInfo& sourceInfo) : info(sourceInfo) {}
}
