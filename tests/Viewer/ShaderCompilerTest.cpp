#include "Viewer/Shader/D3DCompilerHlslCompiler.h"
#include "Viewer/Shader/DxcHlslCompiler.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

namespace Chrivent {
	// 실제 내장 및 패키지 HLSL이 네 API의 컴파일 계약을 모두 만족하는지 검증한다.
	class ShaderCompilerContractTest : public testing::Test {
		// 소스 리소스 루트 아래의 셰이더 절대 경로를 반환한다.
		static std::filesystem::path ResolveShaderPath(const std::filesystem::path& relativePath) {
			return std::filesystem::path(PMXMOD_RESOURCE_SOURCE_DIR) / relativePath;
		}

		// 지정한 셰이더 프로그램을 레거시 DXBC, DXIL과 API별 SPIR-V로 컴파일한다.
		static void ExpectProgramCompiles(const std::filesystem::path& relativePath,
			const char* vertexEntry, const char* pixelEntry,
			const SpirvBindingProfile bindingProfile, const bool invertVertexY) {
			const std::filesystem::path shaderPath = ResolveShaderPath(relativePath);
			const std::wstring wideVertexEntry(vertexEntry, vertexEntry + std::char_traits<char>::length(vertexEntry));
			const std::wstring widePixelEntry(pixelEntry, pixelEntry + std::char_traits<char>::length(pixelEntry));
			const auto legacyVertexResult =
				D3DCompilerHlslCompiler::CompileFile(shaderPath, vertexEntry, "vs_5_0");
			if (!legacyVertexResult)
				ADD_FAILURE() << legacyVertexResult.error().message;
			else
				EXPECT_GT((*legacyVertexResult)->GetBufferSize(), 0);
			const auto legacyPixelResult =
				D3DCompilerHlslCompiler::CompileFile(shaderPath, pixelEntry, "ps_5_0");
			if (!legacyPixelResult)
				ADD_FAILURE() << legacyPixelResult.error().message;
			else
				EXPECT_GT((*legacyPixelResult)->GetBufferSize(), 0);
			const auto dxilVertexResult =
				DxcHlslCompiler::CompileDxil(shaderPath, wideVertexEntry, L"vs_6_0");
			if (!dxilVertexResult)
				ADD_FAILURE() << dxilVertexResult.error().message;
			else
				EXPECT_FALSE(dxilVertexResult->empty());
			const auto dxilPixelResult =
				DxcHlslCompiler::CompileDxil(shaderPath, widePixelEntry, L"ps_6_0");
			if (!dxilPixelResult)
				ADD_FAILURE() << dxilPixelResult.error().message;
			else
				EXPECT_FALSE(dxilPixelResult->empty());
			constexpr SpirvTarget spirvTargets[]{ SpirvTarget::Vulkan, SpirvTarget::OpenGl };
			for (const SpirvTarget spirvTarget : spirvTargets) {
				const auto spirvVertexResult = DxcHlslCompiler::CompileSpirv(shaderPath,
					wideVertexEntry, L"vs_6_0", spirvTarget, bindingProfile, invertVertexY);
				if (!spirvVertexResult)
					ADD_FAILURE() << spirvVertexResult.error().message;
				else
					EXPECT_FALSE(spirvVertexResult->empty());
				const auto spirvPixelResult = DxcHlslCompiler::CompileSpirv(shaderPath,
					widePixelEntry, L"ps_6_0", spirvTarget, bindingProfile);
				if (!spirvPixelResult)
					ADD_FAILURE() << spirvPixelResult.error().message;
				else
					EXPECT_FALSE(spirvPixelResult->empty());
			}
		}

	protected:
		// 내장 장면 프로그램의 모든 API용 셰이더를 컴파일한다.
		static void ExpectSceneProgramCompiles(const std::filesystem::path& relativePath,
			const char* vertexEntry, const char* pixelEntry) {
			ExpectProgramCompiles(relativePath, vertexEntry, pixelEntry,
				SpirvBindingProfile::Scene, false);
		}

		// 패키지 후처리 프로그램의 모든 API용 셰이더를 컴파일한다.
		static void ExpectPostProcessProgramCompiles(const std::filesystem::path& relativePath,
			const char* vertexEntry, const char* pixelEntry) {
			ExpectProgramCompiles(relativePath, vertexEntry, pixelEntry,
				SpirvBindingProfile::PostProcess, true);
		}
	};

	TEST_F(ShaderCompilerContractTest, CompilesInternalSceneProgramsForEveryBackend) {
		ExpectSceneProgramCompiles("internal/shaders/model.hlsl", "VSMain", "PSMain");
		ExpectSceneProgramCompiles("internal/shaders/scene-input.hlsl", "VSVelocity", "PSVelocity");
	}

	TEST_F(ShaderCompilerContractTest, CompilesPackagePostProcessProgramForEveryBackend) {
		ExpectPostProcessProgramCompiles(
			"shaders/sample-grayscale/effects/grayscale/effect.hlsl", "VSMain", "PSMain");
	}
}
