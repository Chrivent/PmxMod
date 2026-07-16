#pragma once

#include <d3d12.h>

namespace Chrivent {
	class Dx12CommandContext;
	class Dx12Pipeline;

	// DX12 Drawer에 현재 명령 목록과 장면 pipeline 바인딩을 제공한다.
	class Dx12DrawContext {
		Dx12CommandContext& commandContext;
		Dx12Pipeline& pipeline;
		bool frameReady = false;
		UINT frameIndex = 0;

	public:
		Dx12DrawContext(Dx12CommandContext& sourceCommandContext, Dx12Pipeline& sourcePipeline);

		UINT GetFrameIndex() const { return frameIndex; }
		bool IsFrameReady() const { return frameReady; }

		// 새 프레임의 인덱스를 저장하고 Drawer 명령 기록을 허용한다.
		void BeginFrame(UINT sourceFrameIndex);
		// 현재 프레임의 Drawer 명령 기록을 막는다.
		void EndFrame();
		// 현재 프레임에서 기록 가능한 명령 목록을 반환한다.
		ID3D12GraphicsCommandList* ResolveCommandList() const;
		// 재질 양면 여부에 맞는 model pipeline을 바인딩한다.
		void BindModelPipeline(bool bothFace) const;
		// 재질 양면 여부에 맞는 depth-only pipeline을 바인딩한다.
		void BindDepthOnlyPipeline(bool bothFace) const;
		// 재질 양면 여부에 맞는 scene velocity pipeline을 바인딩한다.
		void BindSceneVelocityPipeline(bool bothFace) const;
		// 엣지 pipeline을 바인딩한다.
		void BindEdgePipeline() const;
		// 지면 그림자 pipeline을 바인딩한다.
		void BindGroundShadowPipeline() const;
	};
}
