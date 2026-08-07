#include "Viewer/Shader/PostProcessShaderValidator.h"

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/PostProcess/PostProcessRuntimeContract.h"
#include "Viewer/Shader/DxcHlslCompiler.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <exception>
#include <fstream>
#include <spirv_cross/spirv_cross.hpp>
#include <system_error>

namespace Chrivent {
	bool PostProcessShaderValidator::IsPathInside(
		const std::filesystem::path& root, const std::filesystem::path& path) {
		auto rootIterator = root.begin();
		auto pathIterator = path.begin();
		while (rootIterator != root.end()) {
			if (pathIterator == path.end() || *rootIterator != *pathIterator)
				return false;
			++rootIterator;
			++pathIterator;
		}
		return true;
	}

	std::expected<void, std::string> PostProcessShaderValidator::ValidateSourceFile(
		const std::filesystem::path& sourcePath, SourceValidationState& state) {
		std::error_code filesystemError;
		const std::filesystem::path canonicalPath =
			std::filesystem::weakly_canonical(sourcePath, filesystemError);
		if (filesystemError || !IsPathInside(state.includeRoot, canonicalPath)
			|| !std::filesystem::is_regular_file(canonicalPath, filesystemError) || filesystemError) {
			return std::unexpected("HLSL include가 허용된 루트 밖을 가리키거나 존재하지 않습니다: "
				+ sourcePath.string());
		}
		if (!state.validatedFiles.emplace(canonicalPath).second)
			return {};
		std::ifstream stream(canonicalPath, std::ios::binary);
		if (!stream)
			return std::unexpected("HLSL 소스를 열지 못했습니다: " + canonicalPath.string());
		std::string line;
		while (std::getline(stream, line)) {
			size_t position = line.find_first_not_of(" \t");
			if (position == std::string::npos || line[position] != '#')
				continue;
			position = line.find_first_not_of(" \t", position + 1);
			if (position == std::string::npos || line.compare(position, 7, "include") != 0)
				continue;
			position = line.find_first_not_of(" \t", position + 7);
			if (position == std::string::npos || (line[position] != '"' && line[position] != '<')) {
				return std::unexpected("HLSL include는 리터럴 상대 경로여야 합니다: "
					+ canonicalPath.string());
			}
			const char closingDelimiter = line[position] == '"' ? '"' : '>';
			const size_t end = line.find(closingDelimiter, position + 1);
			if (end == std::string::npos || end == position + 1) {
				return std::unexpected("HLSL include 경로가 올바르지 않습니다: "
					+ canonicalPath.string());
			}
			const std::u8string includePathText(line.begin() + position + 1, line.begin() + end);
			const std::filesystem::path relativeInclude(includePathText);
			if (relativeInclude.is_absolute()) {
				return std::unexpected("HLSL include는 패키지 내부의 상대 경로여야 합니다: "
					+ canonicalPath.string());
			}
			const std::filesystem::path includePath = canonicalPath.parent_path() / relativeInclude;
			const auto includeResult = ValidateSourceFile(includePath, state);
			if (!includeResult)
				return includeResult;
		}
		return {};
	}

	std::expected<void, std::string> PostProcessShaderValidator::ValidateIncludes(
		const ShaderProgramDefinition& program) {
		if (program.includeRoot.empty()) {
			return std::unexpected("후처리 HLSL include 루트가 지정되지 않았습니다: "
				+ program.shaderPath.string());
		}
		std::error_code filesystemError;
		SourceValidationState state;
		state.includeRoot = std::filesystem::weakly_canonical(program.includeRoot, filesystemError);
		if (filesystemError || !std::filesystem::is_directory(state.includeRoot, filesystemError)
			|| filesystemError) {
			return std::unexpected("후처리 HLSL include 루트를 확인하지 못했습니다: "
				+ program.includeRoot.string());
		}
		return ValidateSourceFile(program.shaderPath, state);
	}

	std::expected<void, std::string> PostProcessShaderValidator::ValidateStage(
		const ShaderProgramDefinition& program, const std::string& entry, const wchar_t* target,
		const std::vector<bool>& declaredTextureSlots) {
		const std::wstring wideEntry(entry.begin(), entry.end());
		const auto compileResult = DxcHlslCompiler::CompileSpirv(program.shaderPath,
			wideEntry, target, SpirvTarget::Vulkan, SpirvBindingProfile::PostProcess);
		if (!compileResult)
			return std::unexpected(compileResult.error().message);
		spirv_cross::Compiler compiler(*compileResult);
		const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
		for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
			const uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			const bool frameData = descriptorSet == SpirvBindingLayout::frameDataSet
				&& binding == SpirvBindingLayout::frameDataBinding;
			const bool parameterData = descriptorSet == SpirvBindingLayout::parameterDataSet
				&& binding == SpirvBindingLayout::parameterDataBinding;
			if (!frameData && !parameterData) {
				return std::unexpected("후처리 HLSL이 허용되지 않은 constant buffer를 사용합니다: "
					+ program.shaderPath.string() + " entry=" + entry);
			}
		}
		const auto ValidateTextures = [&compiler, &declaredTextureSlots, &program, &entry](
			const spirv_cross::SmallVector<spirv_cross::Resource>& textures) -> std::expected<void, std::string> {
			for (const spirv_cross::Resource& resource : textures) {
				const uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				bool declared = false;
				for (uint32_t slot = 0; slot < declaredTextureSlots.size(); slot++) {
					if (descriptorSet == SpirvBindingLayout::textureSet
						&& binding == SpirvBindingLayout::ResolveTextureBinding(slot)) {
						declared = declaredTextureSlots[slot];
						break;
					}
				}
				if (!declared) {
					return std::unexpected("후처리 HLSL이 pass reads에 없는 texture 슬롯을 사용합니다: "
						+ program.shaderPath.string() + " entry=" + entry);
				}
			}
			return {};
		};
		auto textureResult = ValidateTextures(resources.separate_images);
		if (!textureResult)
			return textureResult;
		textureResult = ValidateTextures(resources.sampled_images);
		if (!textureResult)
			return textureResult;
		for (const spirv_cross::Resource& resource : resources.separate_samplers) {
			const uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			if (descriptorSet != SpirvBindingLayout::textureSet
				|| binding != SpirvBindingLayout::ResolveSamplerBinding(
					PostProcessInputLayout::linearClampSamplerRegister)) {
				return std::unexpected("후처리 HLSL이 허용되지 않은 sampler를 사용합니다: "
					+ program.shaderPath.string() + " entry=" + entry);
			}
		}
		if (!resources.storage_images.empty() || !resources.storage_buffers.empty()
			|| !resources.subpass_inputs.empty() || !resources.push_constant_buffers.empty()) {
			return std::unexpected("후처리 HLSL이 지원하지 않는 GPU 리소스 형식을 사용합니다: "
				+ program.shaderPath.string() + " entry=" + entry);
		}
		return {};
	}

	std::expected<void, std::string> PostProcessShaderValidator::Validate(const EffectPassDefinition& pass) {
		try {
			const auto includeResult = ValidateIncludes(pass.program);
			if (!includeResult)
				return includeResult;
			std::vector declaredTextureSlots(PostProcessInputLayout::maxTextureCount, false);
			for (const EffectPassInputDefinition& input : pass.inputs) {
				if (input.slot >= declaredTextureSlots.size())
					return std::unexpected("후처리 pass texture 슬롯이 허용 범위를 벗어났습니다");
				declaredTextureSlots[input.slot] = true;
			}
			auto stageResult = ValidateStage(
				pass.program, pass.program.vertexEntry, L"vs_6_0", declaredTextureSlots);
			if (!stageResult)
				return stageResult;
			stageResult = ValidateStage(
				pass.program, pass.program.pixelEntry, L"ps_6_0", declaredTextureSlots);
			if (!stageResult)
				return stageResult;
			return {};
		} catch (const std::exception& exception) {
			return std::unexpected("후처리 HLSL 리소스 정보를 해석하지 못했습니다: "
				+ std::string(exception.what()));
		}
	}
}
