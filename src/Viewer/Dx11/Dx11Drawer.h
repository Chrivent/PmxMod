#pragma once

#include "../Drawer.h"

#include <d3d11.h>
#include <glm/glm.hpp>

namespace Chrivent {
	struct Dx11InstanceInfo;
	struct Dx11Texture;

	class Dx11Drawer : public Drawer {
		const Dx11InstanceInfo& info;

		// OpenGL 스타일 clip space를 DirectX depth range로 변환하는 행렬을 반환한다.
		static const glm::mat4& ClipMatrix();
		// 텍스처 유무에 따라 실제 SRV 또는 더미 SRV를 픽셀 셰이더 슬롯에 바인딩한다.
		void BindTexture(
			UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
			int modeIfPresent, int& mode, glm::vec4& mulFactor, glm::vec4& addFactor,
			const glm::vec4& sourceMulFactor, const glm::vec4& sourceAddFactor,
			ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const;

	protected:
		// 일반 메시 패스를 DX11로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 DX11로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 DX11로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		~Dx11Drawer() override = default;

		explicit Dx11Drawer(const Dx11InstanceInfo& sourceInfo);
	};
}
