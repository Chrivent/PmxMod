#pragma once

#include "Viewer/Dx11/Helper/Dx11Shader.h"
#include "Viewer/PostProcess.h"

#include <d3d11.h>
#include <vector>

namespace Chrivent {
	struct Dx11DeviceResources;
	struct Dx11PipelineStates;
	struct Dx11RenderTargets;

	class Dx11PostProcess : public PostProcess {
		Dx11PostProcessShader focusHistoryShader;
		std::vector<Dx11PostProcessShader> postProcessShaders;
		int focusHistoryIndex = 0;
		bool focusHistoryEnabled = false;

		// 화면 크기에 맞는 DX11 viewport를 immediate context에 설정한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
		// DOF용 자동 초점 히스토리 텍스처를 갱신한다.
		void UpdateFocusHistory(const Dx11DeviceResources& resources, const Dx11RenderTargets& targets, int width, int height);

	public:
		Dx11PostProcess() = default;
		~Dx11PostProcess() override = default;

		Dx11PostProcess(const Dx11PostProcess&) = delete;
		Dx11PostProcess& operator=(const Dx11PostProcess&) = delete;
		Dx11PostProcess(Dx11PostProcess&&) = delete;
		Dx11PostProcess& operator=(Dx11PostProcess&&) = delete;

		// 체크된 후처리 effect 목록으로 DX11 fullscreen shader chain을 생성한다.
		bool Load(ID3D11Device* device, const std::vector<const EffectDefinition*>& effects);
		// 후처리용 depth-only pass를 시작한다.
		bool BeginDepthPass(const Dx11DeviceResources& resources, const Dx11RenderTargets& targets,
			const Dx11PipelineStates& pipelineStates, int width, int height) const;
		// 후처리용 depth-only pass를 종료한다.
		static void EndDepthPass(const Dx11DeviceResources& resources);
		// 준비된 후처리 셰이더로 화면 색상을 스왑체인 back buffer에 그린다.
		void Draw(const Dx11DeviceResources& resources, const Dx11RenderTargets& targets,
			const Dx11PipelineStates& pipelineStates, int width, int height);
		// 생성한 DX11 후처리 셰이더 상태를 해제한다.
		void Reset() override;
	};
}
