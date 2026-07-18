#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"

#include "Viewer/Shader/ModernHlslCompiler.h"
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

	bool VulkanShaderStageBuilder::Build(const VulkanDevice& sourceDevice, const ShaderProgramDefinition& program,
		const SpirvBindingProfile bindingProfile, std::string& outError, const bool invertVertexY) {
		outError.clear();
		vertexShader.Reset();
		pixelShader.Reset();
		vertexEntry = program.vertexEntry;
		pixelEntry = program.pixelEntry;
		std::vector<uint32_t> vertexCode;
		std::vector<uint32_t> pixelCode;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		if (!ModernHlslCompiler::CompileSpirv(program.shaderPath, wideVertexEntry, L"vs_6_0", SpirvTarget::Vulkan,
			bindingProfile, vertexCode, outError, invertVertexY)
			|| !ModernHlslCompiler::CompileSpirv(program.shaderPath, widePixelEntry, L"ps_6_0", SpirvTarget::Vulkan,
				bindingProfile, pixelCode, outError))
			return false;
		if (!vertexShader.Initialize(sourceDevice, vertexCode) || !pixelShader.Initialize(sourceDevice, pixelCode))
			return false;
		BuildStageDescriptions();
		return true;
	}
}
