#include "Program/Shader/InternalShaderCatalog.h"
#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Pipeline/Dx11Pipeline.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <array>
#include <gtest/gtest.h>

namespace Chrivent {
	// 창이나 외부 모델 없이 WARP로 실제 내장 셰이더의 재질 색 한 픽셀을 읽는다.
	class ModelShadingTest : public testing::Test {
		using Slot = SceneShaderInputLayout;
		template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		ComPtr<ID3D11Texture2D> output, readback;
		ComPtr<ID3D11Buffer> pixelBuffer;
		Dx11Pipeline pipeline;

		HRESULT CreateBuffer(const UINT bind, const UINT size, const void* data, ID3D11Buffer** buffer) const {
			const D3D11_BUFFER_DESC desc{ .ByteWidth = size, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = bind };
			const D3D11_SUBRESOURCE_DATA initial{ .pSysMem = data };
			return device->CreateBuffer(&desc, data ? &initial : nullptr, buffer);
		}

		HRESULT CreateTextureView(const UINT height, const DXGI_FORMAT format, const void* data,
			const UINT pitch, ID3D11ShaderResourceView** view) const {
			const auto desc = Dx11DescBuilder::MakeTexture2DDesc(1, height, format, D3D11_BIND_SHADER_RESOURCE);
			const D3D11_SUBRESOURCE_DATA initial{ .pSysMem = data, .SysMemPitch = pitch };
			ComPtr<ID3D11Texture2D> texture;
			const HRESULT result = device->CreateTexture2D(&desc, &initial, &texture);
			return SUCCEEDED(result) ? device->CreateShaderResourceView(texture.Get(), nullptr, view) : result;
		}

	protected:
		ModelPixelConstants material{};
		static constexpr glm::vec4 texel = glm::vec4(200, 120, 90, 204) / 255.0f;
		static constexpr glm::vec4 hairTint = glm::vec4(236, 244, 250, 255) / 255.0f;

		void SetUp() override {
			ASSERT_EQ(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
				D3D11_SDK_VERSION, &device, nullptr, &context), S_OK);
			const auto shaders = InternalShaderCatalog::Load(
				std::filesystem::path(PMXMOD_RESOURCE_SOURCE_DIR) / "internal/shaders", true);
			ASSERT_TRUE(shaders) << shaders.error();
			const auto result = pipeline.Initialize(device.Get(), *shaders);
			ASSERT_TRUE(result) << result.error().Format();
			pipeline.BindModel(context.Get());
			context->OMSetBlendState(nullptr, nullptr, 0xffffffff); // 합성 전의 재질 RGBA를 검사한다.
			context->RSSetState(pipeline.ResolveModelRasterizerState(true));

			auto target = Dx11DescBuilder::MakeTexture2DDesc(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_BIND_RENDER_TARGET);
			ASSERT_EQ(device->CreateTexture2D(&target, nullptr, &output), S_OK);
			ComPtr<ID3D11RenderTargetView> rtv;
			ASSERT_EQ(device->CreateRenderTargetView(output.Get(), nullptr, &rtv), S_OK);
			context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
			target.Usage = D3D11_USAGE_STAGING;
			target.BindFlags = 0;
			target.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			ASSERT_EQ(device->CreateTexture2D(&target, nullptr, &readback), S_OK);
			constexpr D3D11_VIEWPORT viewport{ 0, 0, 1, 1, 0, 1 };
			context->RSSetViewports(1, &viewport);

			constexpr glm::vec3 normal(0, 0, 1);
			constexpr ViewerVertex triangle[]{
				{ .position = {-1, -1, 0.5f}, .normal = normal },
				{ .position = {-1,  3, 0.5f}, .normal = normal },
				{ .position = { 3, -1, 0.5f}, .normal = normal }
			};
			ComPtr<ID3D11Buffer> vertices, vertexBuffer;
			ASSERT_EQ(CreateBuffer(D3D11_BIND_VERTEX_BUFFER, sizeof(triangle), triangle, &vertices), S_OK);
			constexpr UINT stride = sizeof(ViewerVertex), offset = 0;
			context->IASetVertexBuffers(0, 1, vertices.GetAddressOf(), &stride, &offset);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			constexpr ModelVertexConstants vs{glm::mat4(1), glm::mat4(1)};
			ASSERT_EQ(CreateBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(vs), &vs, &vertexBuffer), S_OK);
			ASSERT_EQ(CreateBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(material), nullptr, &pixelBuffer), S_OK);
			context->VSSetConstantBuffers(Slot::vertexConstantRegister, 1, vertexBuffer.GetAddressOf());
			context->PSSetConstantBuffers(Slot::pixelConstantRegister, 1, pixelBuffer.GetAddressOf());

			constexpr std::array<uint8_t, 4> base{200, 120, 90, 204};
			constexpr std::array<uint8_t, 8> toon{255, 255, 255, 255, 236, 244, 250, 255};
			ComPtr<ID3D11ShaderResourceView> baseView, toonView;
			ASSERT_EQ(CreateTextureView(1, DXGI_FORMAT_R8G8B8A8_UNORM, base.data(), 4, &baseView), S_OK);
			ASSERT_EQ(CreateTextureView(2, DXGI_FORMAT_R8G8B8A8_UNORM, toon.data(), 4, &toonView), S_OK);
			context->PSSetShaderResources(Slot::baseTextureRegister, 1, baseView.GetAddressOf());
			context->PSSetShaderResources(Slot::toonTextureRegister, 1, toonView.GetAddressOf());
			ID3D11SamplerState* samplers[]{pipeline.GetTextureSampler(), pipeline.GetToonTextureSampler()};
			context->PSSetSamplers(Slot::baseSamplerRegister, 2, samplers);
			material.texMulFactor = material.toonTexMulFactor = glm::vec4(1);
			material.diffuseAlpha = glm::vec4(1);
			material.ambientSpecularPower = glm::vec4(0.5f, 0.5f, 0.5f, 0);
			material.lightColor = glm::vec4(154.0f / 255.0f);
			material.textureModes.x = 2;
		}

		void ExpectPixel(const glm::vec3& lightDirection, const glm::vec4& expected, const float tolerance = 0.0001f) {
			SCOPED_TRACE(testing::Message() << "lightZ=" << lightDirection.z);
			material.lightDir = glm::vec4(lightDirection, 0.0f);
			context->UpdateSubresource(pixelBuffer.Get(), 0, nullptr, &material, 0, 0);
			context->Draw(3, 0);
			context->CopyResource(readback.Get(), output.Get());
			D3D11_MAPPED_SUBRESOURCE mapped{};
			ASSERT_EQ(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), S_OK);
			const auto* actual = static_cast<const float*>(mapped.pData);
			for (int channel = 0; channel < 4; ++channel)
				EXPECT_NEAR(actual[channel], expected[channel], tolerance) << "channel=" << channel;
			context->Unmap(readback.Get(), 0);
		}
	};

	TEST_F(ModelShadingTest, UnassignedToonIsWhiteRegardlessOfLightDirection) {
		ExpectPixel({0, 0, -1}, texel);
		ExpectPixel({0, 0, 1}, texel);
	}

	TEST_F(ModelShadingTest, AuthoredToonRampFollowsSurfaceNormal) {
		material.textureModes.y = 1;
		ExpectPixel({0, 0, -1}, texel);
		// N·L = -1/4이면 2행짜리 툰의 아래 색을 3/4 섞는다.
		ExpectPixel({glm::sqrt(15.0f), 0, 1}, texel * glm::mix(glm::vec4(1), hairTint, 0.75f), 0.5f / 255.0f);
		ExpectPixel({0, 0, 1}, texel * hairTint);
	}

	TEST_F(ModelShadingTest, MissingToonKeepsMaterialAndLightColor) {
		material.textureModes.x = 0;
		material.ambientSpecularPower = {0.1f, 0.2f, 0.3f, 0};
		material.diffuseAlpha = {0.5f, 0.5f, 0.25f, 0.8f};
		material.lightColor = {0.8f, 0.4f, 0.4f, 0};
		ExpectPixel({0, 0, 1}, {0.5f, 0.4f, 0.4f, 0.8f});
	}
}
