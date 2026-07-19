#include "Viewer/PostProcess/OpenGlPostProcess.h"

#include "Viewer/Error/OpenGlError.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	OpenGlPostProcess::~OpenGlPostProcess() {
		OpenGlPostProcess::ResetResources();
	}

	GraphicsResult<void> OpenGlPostProcess::CreateEffectResources() {
		ResetEffectResources();
		OpenGlError::Clear();
		const auto& plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			auto& [framebuffers, textures] = resources[resourceIndex];
			const GLsizei textureCount = plan.lifetime == EffectResourceLifetime::History ? 2 : 1;
			const int width = ResolveResourceExtent(targetWidth, plan, true);
			const int height = ResolveResourceExtent(targetHeight, plan, false);
			const GLenum format = plan.format == EffectTextureFormat::Rgba8Unorm
				? GL_RGBA8 : plan.format == EffectTextureFormat::Rgba16Float ? GL_RGBA16F : GL_RGBA32F;
			glCreateFramebuffers(textureCount, framebuffers);
			glCreateTextures(GL_TEXTURE_2D, textureCount, textures);
			for (GLsizei index = 0; index < textureCount; index++) {
				if (framebuffers[index] == 0 || textures[index] == 0) {
					const GLenum result = OpenGlError::Take();
					return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
						GraphicsErrorCode::ResourceCreationFailed, "후처리 effect resource 생성",
						"OpenGL 후처리 effect framebuffer 또는 texture 객체를 만들지 못했습니다",
						result, result != GL_NO_ERROR));
				}
				glTextureStorage2D(textures[index], 1, format, width, height);
				glTextureParameteri(textures[index], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTextureParameteri(textures[index], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTextureParameteri(textures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(textures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(
					framebuffers[index], GL_COLOR_ATTACHMENT0, textures[index], 0);
				const GLenum status = glCheckNamedFramebufferStatus(framebuffers[index], GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
						GraphicsErrorCode::ResourceCreationFailed, "후처리 effect framebuffer 생성",
						"OpenGL 후처리 effect framebuffer가 완전하지 않습니다", status, true));
				}
			}
		}
		const GLenum result = OpenGlError::Take();
		if (result != GL_NO_ERROR) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 effect resource 생성",
				"OpenGL 후처리 effect resource를 초기화하지 못했습니다", result, true));
		}
		ResetHistory();
		return {};
	}

	void OpenGlPostProcess::InitializeHistories() {
		const auto& plans = GetResourcePlans();
		constexpr float clearColor[4]{};
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			Resource& resource = resources[index];
			if (!NeedsHistoryInitialization(index))
				continue;
			glClearNamedFramebufferfv(resource.framebuffers[0], GL_COLOR, 0, clearColor);
			glClearNamedFramebufferfv(resource.framebuffers[1], GL_COLOR, 0, clearColor);
			MarkHistoryInitialized(index);
		}
	}

	GLuint OpenGlPostProcess::ResolveInputTexture(const PostProcessPassInputRoute& input) const {
		if (input.kind == PostProcessInputKind::SceneColor)
			return sceneColorTexture;
		if (input.kind == PostProcessInputKind::SceneDepth)
			return postProcessDepthTexture;
		if (input.kind == PostProcessInputKind::SceneVelocity)
			return postProcessVelocityTexture;
		if (input.resourceIndex >= resources.size())
			return 0;
		const auto& [framebuffers, textures] = resources[input.resourceIndex];
		return textures[ResolveResourceReadIndex(input.resourceIndex)];
	}

	GLuint OpenGlPostProcess::ResolveOutputFramebuffer(const PostProcessPassRoute& route) const {
		if (route.outputKind == PostProcessOutputKind::Present)
			return 0;
		if (route.outputResourceIndex >= resources.size())
			return 0;
		const auto& [framebuffers, textures] = resources[route.outputResourceIndex];
		return framebuffers[ResolveResourceWriteIndex(route.outputResourceIndex)];
	}

	void OpenGlPostProcess::ResetEffectResources() {
		const auto& plans = GetResourcePlans();
		for (size_t index = 0; index < resources.size(); index++) {
			const GLsizei count = index < plans.size() && plans[index].lifetime == EffectResourceLifetime::History ? 2 : 1;
			glDeleteTextures(count, resources[index].textures);
			glDeleteFramebuffers(count, resources[index].framebuffers);
		}
		resources.clear();
	}

	void OpenGlPostProcess::ResetTargets() {
		ResetEffectResources();
		if (sceneDepthStencil != 0)
			glDeleteRenderbuffers(1, &sceneDepthStencil);
		if (sceneColorMsaa != 0)
			glDeleteRenderbuffers(1, &sceneColorMsaa);
		if (sceneColorTexture != 0)
			glDeleteTextures(1, &sceneColorTexture);
		if (postProcessDepthTexture != 0)
			glDeleteTextures(1, &postProcessDepthTexture);
		if (postProcessVelocityTexture != 0)
			glDeleteTextures(1, &postProcessVelocityTexture);
		if (postProcessDepthFramebuffer != 0)
			glDeleteFramebuffers(1, &postProcessDepthFramebuffer);
		if (resolveFramebuffer != 0)
			glDeleteFramebuffers(1, &resolveFramebuffer);
		if (sceneFramebuffer != 0)
			glDeleteFramebuffers(1, &sceneFramebuffer);
		if (frameDataBuffer != 0)
			glDeleteBuffers(1, &frameDataBuffer);
		if (parameterDataBuffer != 0)
			glDeleteBuffers(1, &parameterDataBuffer);
		sceneDepthStencil = 0;
		sceneColorMsaa = 0;
		sceneColorTexture = 0;
		postProcessDepthTexture = 0;
		postProcessVelocityTexture = 0;
		postProcessDepthFramebuffer = 0;
		resolveFramebuffer = 0;
		sceneFramebuffer = 0;
		frameDataBuffer = 0;
		parameterDataBuffer = 0;
		postProcessSampleCount = 1;
		targetWidth = 0;
		targetHeight = 0;
	}

	void OpenGlPostProcess::ResetPrograms() {
		postProcessPrograms.clear();
		ResetHistory();
	}

	GraphicsResult<void> OpenGlPostProcess::InitializeTargets(
		const int width, const int height, const int sampleCount) {
		ResetTargets();
		if (width <= 0 || height <= 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidArgument, "후처리 target 생성",
				"OpenGL 후처리 target 크기가 올바르지 않습니다"));
		}
		targetWidth = width;
		targetHeight = height;
		OpenGlError::Clear();
		glCreateBuffers(1, &frameDataBuffer);
		glCreateBuffers(1, &parameterDataBuffer);
		const GLenum bufferResult = OpenGlError::Take();
		if (frameDataBuffer == 0 || parameterDataBuffer == 0 || bufferResult != GL_NO_ERROR) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 constant buffer 생성",
				"OpenGL 후처리 constant buffer 객체를 만들지 못했습니다",
				bufferResult, bufferResult != GL_NO_ERROR));
		}
		OpenGlError::Clear();
		glNamedBufferData(frameDataBuffer, sizeof(PostProcessFrameData), nullptr, GL_DYNAMIC_DRAW);
		glNamedBufferData(parameterDataBuffer, sizeof(PostProcessParameterData), nullptr, GL_DYNAMIC_DRAW);
		postProcessSampleCount = std::max<GLsizei>(1, sampleCount);
		glCreateFramebuffers(1, &sceneFramebuffer);
		glCreateRenderbuffers(1, &sceneColorMsaa);
		glCreateRenderbuffers(1, &sceneDepthStencil);
		if (postProcessSampleCount > 1) {
			glNamedRenderbufferStorageMultisample(sceneColorMsaa, postProcessSampleCount, GL_RGBA8, width, height);
			glNamedRenderbufferStorageMultisample(
				sceneDepthStencil, postProcessSampleCount, GL_DEPTH24_STENCIL8, width, height);
		} else {
			glNamedRenderbufferStorage(sceneColorMsaa, GL_RGBA8, width, height);
			glNamedRenderbufferStorage(sceneDepthStencil, GL_DEPTH24_STENCIL8, width, height);
		}
		glNamedFramebufferRenderbuffer(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColorMsaa);
		glNamedFramebufferRenderbuffer(
			sceneFramebuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthStencil);
		glCreateFramebuffers(1, &resolveFramebuffer);
		glCreateTextures(GL_TEXTURE_2D, 1, &sceneColorTexture);
		glTextureStorage2D(sceneColorTexture, 1, GL_RGBA8, width, height);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glNamedFramebufferTexture(resolveFramebuffer, GL_COLOR_ATTACHMENT0, sceneColorTexture, 0);
		if (RequiresDepth() || RequiresVelocity()) {
			glCreateFramebuffers(1, &postProcessDepthFramebuffer);
			glCreateTextures(GL_TEXTURE_2D, 1, &postProcessDepthTexture);
			glTextureStorage2D(postProcessDepthTexture, 1, GL_DEPTH_COMPONENT24, width, height);
			glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glNamedFramebufferTexture(
				postProcessDepthFramebuffer, GL_DEPTH_ATTACHMENT, postProcessDepthTexture, 0);
			if (RequiresVelocity()) {
				glCreateTextures(GL_TEXTURE_2D, 1, &postProcessVelocityTexture);
				glTextureStorage2D(postProcessVelocityTexture, 1, GL_RG16F, width, height);
				glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(postProcessDepthFramebuffer,
					GL_COLOR_ATTACHMENT0, postProcessVelocityTexture, 0);
			}
			glNamedFramebufferDrawBuffer(postProcessDepthFramebuffer,
				RequiresVelocity() ? GL_COLOR_ATTACHMENT0 : GL_NONE);
			glNamedFramebufferReadBuffer(postProcessDepthFramebuffer, GL_NONE);
		}
		if (postProcessVao == 0)
			glCreateVertexArrays(1, &postProcessVao);
		if (sceneFramebuffer == 0 || sceneColorMsaa == 0 || sceneDepthStencil == 0
			|| resolveFramebuffer == 0 || sceneColorTexture == 0 || postProcessVao == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 target 생성",
				"OpenGL 후처리 framebuffer, texture 또는 vertex array 객체를 만들지 못했습니다"));
		}
		GLenum status = glCheckNamedFramebufferStatus(sceneFramebuffer, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 scene framebuffer 생성",
				"OpenGL 후처리 scene framebuffer가 완전하지 않습니다", status, true));
		}
		status = glCheckNamedFramebufferStatus(resolveFramebuffer, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 resolve framebuffer 생성",
				"OpenGL 후처리 resolve framebuffer가 완전하지 않습니다", status, true));
		}
		if (postProcessDepthFramebuffer != 0) {
			status = glCheckNamedFramebufferStatus(postProcessDepthFramebuffer, GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth framebuffer 생성",
					"OpenGL 후처리 depth framebuffer가 완전하지 않습니다", status, true));
			}
		}
		const GLenum result = OpenGlError::Take();
		if (result != GL_NO_ERROR) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 target 생성",
				"OpenGL 후처리 target을 초기화하지 못했습니다", result, true));
		}
		return CreateEffectResources();
	}

	GraphicsResult<void> OpenGlPostProcess::CreatePrograms() {
		ResetPrograms();
		for (const auto& program : GetShaderPrograms()) {
			OpenGlPostProcessShaderProgram postProcessProgram;
			const auto result = postProcessProgram.Initialize(program);
			if (!result)
				return std::unexpected(result.error());
			postProcessPrograms.push_back(std::move(postProcessProgram));
		}
		return {};
	}

	void OpenGlPostProcess::SwapResources(OpenGlPostProcess& other) noexcept {
		std::swap(sceneFramebuffer, other.sceneFramebuffer);
		std::swap(sceneColorMsaa, other.sceneColorMsaa);
		std::swap(resolveFramebuffer, other.resolveFramebuffer);
		std::swap(sceneColorTexture, other.sceneColorTexture);
		std::swap(postProcessDepthFramebuffer, other.postProcessDepthFramebuffer);
		std::swap(postProcessDepthTexture, other.postProcessDepthTexture);
		std::swap(postProcessVelocityTexture, other.postProcessVelocityTexture);
		std::swap(sceneDepthStencil, other.sceneDepthStencil);
		std::swap(postProcessVao, other.postProcessVao);
		std::swap(frameDataBuffer, other.frameDataBuffer);
		std::swap(parameterDataBuffer, other.parameterDataBuffer);
		std::swap(postProcessSampleCount, other.postProcessSampleCount);
		resources.swap(other.resources);
		postProcessPrograms.swap(other.postProcessPrograms);
		std::swap(targetWidth, other.targetWidth);
		std::swap(targetHeight, other.targetHeight);
	}

	GraphicsResult<void> OpenGlPostProcess::Configure(const int width, const int height,
		const int sampleCount,
		const std::vector<const EffectRuntimeDefinition*>& effects) {
		OpenGlPostProcess candidate;
		const auto planResult = candidate.SetEffects(effects);
		if (!planResult) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ContractViolation, "후처리 실행 계획 생성", planResult.error()));
		}
		if (candidate.HasEffects()) {
			const auto targetResult = candidate.InitializeTargets(width, height, sampleCount);
			if (!targetResult)
				return std::unexpected(targetResult.error());
			const auto programResult = candidate.CreatePrograms();
			if (!programResult)
				return std::unexpected(programResult.error());
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}

	GraphicsResult<void> OpenGlPostProcess::BeginSceneInputPass(const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || postProcessDepthFramebuffer == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"OpenGL 후처리 장면 입력 target이 준비되지 않았습니다"));
		}
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessDepthFramebuffer);
		glViewport(0, 0, width, height);
		glColorMask(RequiresVelocity(), RequiresVelocity(), GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		constexpr float velocityClear[4]{};
		if (RequiresVelocity())
			glClearBufferfv(GL_COLOR, 0, velocityClear);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_POLYGON_OFFSET_FILL);
		return {};
	}

	GraphicsResult<void> OpenGlPostProcess::EndSceneInputPass() {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return {};
	}

	GraphicsResult<void> OpenGlPostProcess::Draw(
		const int width, const int height, const PostProcessFrameData& frameData) {
		if (!HasEffects())
			return {};
		if (sceneFramebuffer == 0 || resolveFramebuffer == 0 || sceneColorTexture == 0
			|| frameDataBuffer == 0 || parameterDataBuffer == 0 || postProcessVao == 0
			|| !IsPassCountCompatible(postProcessPrograms.size())) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"OpenGL 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		BeginHistoryFrame();
		glNamedBufferSubData(frameDataBuffer, 0, sizeof(frameData), &frameData);
		glBindBufferBase(GL_UNIFORM_BUFFER, PostProcessInputLayout::frameDataRegister, frameDataBuffer);
		glBlitNamedFramebuffer(sceneFramebuffer, resolveFramebuffer,
			0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glBindVertexArray(postProcessVao);
		InitializeHistories();
		const auto& routes = GetPassRoutes();
		glBindBufferBase(GL_UNIFORM_BUFFER,
			PostProcessInputLayout::parameterDataRegister, parameterDataBuffer);
		size_t parameterEffectIndex = routes.size();
		for (size_t index = 0; index < routes.size(); index++) {
			const PostProcessPassRoute& route = routes[index];
			if (route.outputKind == PostProcessOutputKind::Resource
				&& route.outputResourceIndex >= resources.size()) {
				DiscardHistoryFrame();
				return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
					GraphicsErrorCode::ContractViolation, "후처리 출력 framebuffer 조회",
					"OpenGL 후처리 pass의 출력 framebuffer를 찾지 못했습니다"));
			}
			if (parameterEffectIndex != route.effectIndex) {
				const PostProcessParameterData& parameterData = GetParameterData(route);
				glNamedBufferSubData(parameterDataBuffer, 0, sizeof(parameterData), &parameterData);
				parameterEffectIndex = route.effectIndex;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, ResolveOutputFramebuffer(route));
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			glViewport(0, 0, outputWidth, outputHeight);
			glUseProgram(postProcessPrograms[index].GetProgram());
			for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++)
				glBindTextureUnit(SpirvBindingLayout::ResolveTextureBinding(slot), sceneColorTexture);
			for (const auto& input : route.inputs) {
				const GLuint texture = ResolveInputTexture(input);
				if (texture == 0) {
					DiscardHistoryFrame();
					return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
						GraphicsErrorCode::ContractViolation, "후처리 입력 texture 조회",
						"OpenGL 후처리 pass의 입력 texture를 찾지 못했습니다"));
				}
				glBindTextureUnit(SpirvBindingLayout::ResolveTextureBinding(input.slot), texture);
			}
			glDrawArrays(GL_TRIANGLES, 0, 3);
			AdvanceHistory(route);
		}
		return {};
	}

	void OpenGlPostProcess::ResetResources() {
		ResetPrograms();
		ResetTargets();
		if (postProcessVao != 0)
			glDeleteVertexArrays(1, &postProcessVao);
		postProcessVao = 0;
	}
}
