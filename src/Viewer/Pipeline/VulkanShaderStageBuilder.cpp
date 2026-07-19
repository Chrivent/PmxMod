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

	GraphicsError::Result<void> VulkanShaderStageBuilder::Build(
		const VulkanDevice& sourceDevice, const ShaderProgramDefinition& program,
		const SpirvBindingProfile bindingProfile, const bool invertVertexY) {
		vertexShader.Reset();
		pixelShader.Reset();
		vertexEntry = program.vertexEntry;
		pixelEntry = program.pixelEntry;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		auto vertexCodeResult = DxcHlslCompiler::CompileSpirv(
			program.shaderPath, wideVertexEntry, L"vs_6_0",
			SpirvTarget::Vulkan, bindingProfile, invertVertexY);
		if (!vertexCodeResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::EffectConfigurationFailed, "vertex SPIR-V 셰이더 컴파일",
				std::move(vertexCodeResult.error().message)));
		}
		auto pixelCodeResult = DxcHlslCompiler::CompileSpirv(
			program.shaderPath, widePixelEntry, L"ps_6_0",
			SpirvTarget::Vulkan, bindingProfile);
		if (!pixelCodeResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::EffectConfigurationFailed, "pixel SPIR-V 셰이더 컴파일",
				std::move(pixelCodeResult.error().message)));
		}
		const std::vector<uint32_t>& vertexCode = *vertexCodeResult;
		const std::vector<uint32_t>& pixelCode = *pixelCodeResult;
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
