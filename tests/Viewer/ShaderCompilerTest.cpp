#include "Program/Shader/InternalShaderCatalog.h"
#include "Program/Shader/ShaderPackage.h"
#include "Viewer/Shader/D3DCompilerHlslCompiler.h"
#include "Viewer/Shader/DxcHlslCompiler.h"
#include "Viewer/Shader/OpenGlShader.h"
#include "Viewer/Shader/PostProcessShaderValidator.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

namespace Chrivent {
	// 실제 내장 및 패키지 HLSL이 네 API의 컴파일 계약을 모두 만족하는지 검증한다.
	class ShaderCompilerContractTest : public testing::Test {
	protected:
		// 이름이 겹치지 않는 검증용 임시 폴더를 비운 상태로 준비한다.
		static std::filesystem::path PrepareShaderDirectory(const char* name) {
			const std::filesystem::path directory = std::filesystem::temp_directory_path() / name;
			std::error_code filesystemError;
			std::filesystem::remove_all(directory, filesystemError);
			filesystemError.clear();
			std::filesystem::create_directories(directory, filesystemError);
			EXPECT_FALSE(filesystemError);
			return directory;
		}

		// 검증용 HLSL 텍스트를 지정한 경로에 기록한다.
		static void WriteShader(const std::filesystem::path& path, const char* source) {
			std::ofstream stream(path, std::ios::binary);
			ASSERT_TRUE(stream.good());
			stream << source;
			ASSERT_TRUE(stream.good());
		}

		// 경로와 진입점이 같은 프로그램을 중복하지 않고 수집한다.
		static void AppendProgram(std::vector<ShaderProgramDefinition>& programs,
			const ShaderProgramDefinition& program) {
			const auto existing = std::ranges::find_if(programs,
				[&program](const ShaderProgramDefinition& candidate) {
					return candidate.shaderPath == program.shaderPath
						&& candidate.vertexEntry == program.vertexEntry
						&& candidate.pixelEntry == program.pixelEntry;
				});
			if (existing == programs.end())
				programs.emplace_back(program);
		}

		// 역할별 장면 셰이더 계약의 모든 프로그램을 수집한다.
		static void AppendScenePrograms(std::vector<ShaderProgramDefinition>& programs,
			const SceneShaderRuntimeContract& contract) {
			AppendProgram(programs, contract.builtIn.model);
			AppendProgram(programs, contract.builtIn.edge);
			AppendProgram(programs, contract.builtIn.groundShadow);
			AppendProgram(programs, contract.sceneInput.depth);
			AppendProgram(programs, contract.sceneInput.velocity);
		}

		// OpenGL용 SPIR-V가 실제 GLSL 변환 단계까지 통과하는지 확인한다.
		static void ExpectOpenGlTranslation(const std::vector<uint32_t>& code) {
			const auto translationResult = OpenGlProgramBuilder::ConvertSpirvToGlsl(code);
			if (!translationResult)
				ADD_FAILURE() << translationResult.error().Format();
			else
				EXPECT_FALSE(translationResult->empty());
		}

		// 지정한 셰이더 프로그램을 레거시 DXBC, DXIL과 API별 SPIR-V로 컴파일한다.
		static void ExpectProgramCompiles(const ShaderProgramDefinition& program,
			const SpirvBindingProfile bindingProfile, const bool invertVertexY) {
			const std::wstring wideVertexEntry(program.vertexEntry.begin(), program.vertexEntry.end());
			const std::wstring widePixelEntry(program.pixelEntry.begin(), program.pixelEntry.end());
			const auto legacyVertexResult =
				D3DCompilerHlslCompiler::CompileFile(program.shaderPath, program.vertexEntry.c_str(), "vs_5_0");
			if (!legacyVertexResult)
				ADD_FAILURE() << legacyVertexResult.error().message;
			else
				EXPECT_GT((*legacyVertexResult)->GetBufferSize(), 0);
			const auto legacyPixelResult =
				D3DCompilerHlslCompiler::CompileFile(program.shaderPath, program.pixelEntry.c_str(), "ps_5_0");
			if (!legacyPixelResult)
				ADD_FAILURE() << legacyPixelResult.error().message;
			else
				EXPECT_GT((*legacyPixelResult)->GetBufferSize(), 0);
			const auto dxilVertexResult =
				DxcHlslCompiler::CompileDxil(program.shaderPath, wideVertexEntry, L"vs_6_0");
			if (!dxilVertexResult)
				ADD_FAILURE() << dxilVertexResult.error().message;
			else
				EXPECT_FALSE(dxilVertexResult->empty());
			const auto dxilPixelResult =
				DxcHlslCompiler::CompileDxil(program.shaderPath, widePixelEntry, L"ps_6_0");
			if (!dxilPixelResult)
				ADD_FAILURE() << dxilPixelResult.error().message;
			else
				EXPECT_FALSE(dxilPixelResult->empty());
			constexpr SpirvTarget spirvTargets[]{ SpirvTarget::Vulkan, SpirvTarget::OpenGl };
			for (const SpirvTarget spirvTarget : spirvTargets) {
				const auto spirvVertexResult = DxcHlslCompiler::CompileSpirv(program.shaderPath,
					wideVertexEntry, L"vs_6_0", spirvTarget, bindingProfile, invertVertexY);
				if (!spirvVertexResult)
					ADD_FAILURE() << spirvVertexResult.error().message;
				else {
					EXPECT_FALSE(spirvVertexResult->empty());
					if (spirvTarget == SpirvTarget::OpenGl)
						ExpectOpenGlTranslation(*spirvVertexResult);
				}
				const auto spirvPixelResult = DxcHlslCompiler::CompileSpirv(program.shaderPath,
					widePixelEntry, L"ps_6_0", spirvTarget, bindingProfile);
				if (!spirvPixelResult)
					ADD_FAILURE() << spirvPixelResult.error().message;
				else {
					EXPECT_FALSE(spirvPixelResult->empty());
					if (spirvTarget == SpirvTarget::OpenGl)
						ExpectOpenGlTranslation(*spirvPixelResult);
				}
			}
		}

		// 소스 리소스 루트 아래의 절대 경로를 반환한다.
		static std::filesystem::path GetResourcePath(const std::filesystem::path& relativePath) {
			return std::filesystem::path(PMXMOD_RESOURCE_SOURCE_DIR) / relativePath;
		}

		// 내장 장면 프로그램의 모든 API용 셰이더를 컴파일한다.
		static void ExpectSceneProgramCompiles(const ShaderProgramDefinition& program) {
			ExpectProgramCompiles(program, SpirvBindingProfile::Scene, false);
		}

		// 패키지 후처리 프로그램의 모든 API용 셰이더를 컴파일한다.
		static void ExpectPostProcessProgramCompiles(const ShaderProgramDefinition& program) {
			ExpectProgramCompiles(program, SpirvBindingProfile::PostProcess, true);
		}

		// 테스트 본문에서 역할별 장면 프로그램을 중복 없이 수집한다.
		static void CollectScenePrograms(std::vector<ShaderProgramDefinition>& programs,
			const SceneShaderRuntimeContract& contract) {
			AppendScenePrograms(programs, contract);
		}

		// 테스트 본문에서 패키지 프로그램을 중복 없이 수집한다.
		static void CollectProgram(std::vector<ShaderProgramDefinition>& programs,
			const ShaderProgramDefinition& program) {
			AppendProgram(programs, program);
		}
	};

	TEST_F(ShaderCompilerContractTest, CompilesInternalSceneProgramsForEveryBackend) {
		const auto regularContractResult =
			InternalShaderCatalog::Load(GetResourcePath("internal/shaders"), false);
		ASSERT_TRUE(regularContractResult.has_value());
		const auto invertedContractResult =
			InternalShaderCatalog::Load(GetResourcePath("internal/shaders"), true);
		ASSERT_TRUE(invertedContractResult.has_value());
		std::vector<ShaderProgramDefinition> programs;
		CollectScenePrograms(programs, *regularContractResult);
		CollectScenePrograms(programs, *invertedContractResult);
		for (const auto& program : programs)
			ExpectSceneProgramCompiles(program);
	}

	TEST_F(ShaderCompilerContractTest, CompilesPackagePostProcessProgramsForEveryBackend) {
		const auto [packages, errors] =
			ShaderPackageLoader::Discover(GetResourcePath("shaders"));
		for (const ShaderPackageError& error : errors)
			ADD_FAILURE() << error.Format();
		ASSERT_TRUE(errors.empty());
		ASSERT_FALSE(packages.empty());
		std::vector<ShaderProgramDefinition> programs;
		for (const ShaderPackage& package : packages) {
			for (const EffectDefinition& effect : package.effects) {
				for (const EffectPassDefinition& pass : effect.runtime.passes)
					CollectProgram(programs, pass.program);
			}
		}
		ASSERT_FALSE(programs.empty());
		for (const auto& program : programs) {
			SCOPED_TRACE(program.shaderPath.string());
			ExpectPostProcessProgramCompiles(program);
		}
	}

	TEST_F(ShaderCompilerContractTest, RejectsIncludeOutsidePackageRoot) {
		const std::filesystem::path directory = PrepareShaderDirectory("PmxModShaderIncludeEscapeTest");
		const std::filesystem::path outsidePath = directory.parent_path() / "PmxModOutsideShaderInclude.hlsli";
		WriteShader(outsidePath, "float4 ResolveColor() { return 1.0; }\n");
		const std::filesystem::path shaderPath = directory / "effect.hlsl";
		WriteShader(shaderPath, "#include \"../PmxModOutsideShaderInclude.hlsli\"\n");
		const EffectPassDefinition pass{
			.program = {
				.shaderPath = shaderPath,
				.includeRoot = directory,
				.vertexEntry = "VSMain",
				.pixelEntry = "PSMain"
			}
		};
		EXPECT_FALSE(PostProcessShaderValidator::Validate(pass).has_value());
		std::error_code filesystemError;
		std::filesystem::remove_all(directory, filesystemError);
		std::filesystem::remove(outsidePath, filesystemError);
	}

	TEST_F(ShaderCompilerContractTest, RejectsTextureBindingMissingFromPassReads) {
		const std::filesystem::path directory = PrepareShaderDirectory("PmxModShaderBindingTest");
		const std::filesystem::path shaderPath = directory / "effect.hlsl";
		WriteShader(shaderPath,
			"Texture2D InputTexture : register(t1);\n"
			"SamplerState LinearClamp : register(s0);\n"
			"struct VertexOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };\n"
			"VertexOutput VSMain(uint id : SV_VertexID) {\n"
			"    VertexOutput output;\n"
			"    output.uv = float2((id << 1) & 2, id & 2);\n"
			"    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
			"    return output;\n"
			"}\n"
			"float4 PSMain(VertexOutput input) : SV_TARGET {\n"
			"    return InputTexture.Sample(LinearClamp, input.uv);\n"
			"}\n");
		const EffectPassDefinition pass{
			.program = {
				.shaderPath = shaderPath,
				.includeRoot = directory,
				.vertexEntry = "VSMain",
				.pixelEntry = "PSMain"
			},
			.inputs = { { .slot = 0, .kind = EffectPassInputKind::SceneColor } }
		};
		EXPECT_FALSE(PostProcessShaderValidator::Validate(pass).has_value());
		std::error_code filesystemError;
		std::filesystem::remove_all(directory, filesystemError);
	}
}
