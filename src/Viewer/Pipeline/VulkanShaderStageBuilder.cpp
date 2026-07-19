#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"

#include "Viewer/Shader/DxcHlslCompiler.h"

#include <utility>
#include <vector>

namespace Chrivent {
	void VulkanShaderStageBuilder::BuildStageDescriptions() {
		stages[0] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShader.GetShaderModule(),
			.pName = vertexEntry.c_str()
		};
		stages[1] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = pixelShader.GetShaderModule(),
			.pName = pixelEntry.c_str()
		};
	}

	GraphicsResult<void> VulkanShaderStageBuilder::Build(
		const VulkanDevice& sourceDevice, const ShaderProgramDefinition& program,
		const SpirvBindingProfile bindingProfile, const bool invertVertexY) {
		vertexShader.Reset();
		pixelShader.Reset();
		vertexEntry = program.vertexEntry;
		pixelEntry = program.pixelEntry;
		std::vector<uint32_t> vertexCode;
		std::vector<uint32_t> pixelCode;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		std::string error;
		if (!DxcHlslCompiler::CompileSpirv(program.shaderPath, wideVertexEntry, L"vs_6_0", SpirvTarget::Vulkan,
			bindingProfile, vertexCode, error, invertVertexY)
			|| !DxcHlslCompiler::CompileSpirv(program.shaderPath, widePixelEntry, L"ps_6_0", SpirvTarget::Vulkan,
				bindingProfile, pixelCode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::EffectConfigurationFailed, "SPIR-V 셰이더 컴파일",
				error.empty() ? "Vulkan용 SPIR-V 셰이더를 컴파일하지 못했습니다" : std::move(error)));
		}
		auto result = vertexShader.Initialize(sourceDevice, vertexCode);
		if (!result)
			return result;
		result = pixelShader.Initialize(sourceDevice, pixelCode);
		if (!result)
			return result;
		BuildStageDescriptions();
		return {};
	}
}
