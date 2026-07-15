#include "Viewer/PostProcess/OpenGlPostProcess.h"

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Viewer/Viewer.h"

#include <algorithm>

namespace Chrivent {
	OpenGlPostProcess::~OpenGlPostProcess() {
		OpenGlPostProcess::ResetResources();
	}

	bool OpenGlPostProcess::CreateEffectResources() {
		ResetEffectResources();
		const auto& plans = ResolveResourcePlans();
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
				glTextureStorage2D(textures[index], 1, format, width, height);
				glTextureParameteri(textures[index], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTextureParameteri(textures[index], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTextureParameteri(textures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(textures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(
					framebuffers[index], GL_COLOR_ATTACHMENT0, textures[index], 0);
				if (glCheckNamedFramebufferStatus(framebuffers[index], GL_FRAMEBUFFER)
					!= GL_FRAMEBUFFER_COMPLETE)
					return false;
			}
		}
		ResetHistory();
		return true;
	}

	void OpenGlPostProcess::InitializeHistories() {
		const auto& plans = ResolveResourcePlans();
		constexpr float clearColor[4]{};
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			OpenGlPostProcessResource& resource = resources[index];
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
			return sceneColorTexture;
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
		const auto& plans = ResolveResourcePlans();
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

	void OpenGlPostProcess::ResetShaders() {
		postProcessShaders.clear();
		ResetHistory();
	}

	bool OpenGlPostProcess::InitializeTargets(
		const int width, const int height, const int msaaSamples, const uint32_t maxSampleCount) {
		ResetTargets();
		if (width <= 0 || height <= 0)
			return false;
		targetWidth = width;
		targetHeight = height;
		glCreateBuffers(1, &frameDataBuffer);
		glCreateBuffers(1, &parameterDataBuffer);
		if (frameDataBuffer == 0 || parameterDataBuffer == 0)
			return false;
		glNamedBufferData(frameDataBuffer, sizeof(PostProcessFrameData), nullptr, GL_DYNAMIC_DRAW);
		glNamedBufferData(parameterDataBuffer, sizeof(PostProcessParameterData), nullptr, GL_DYNAMIC_DRAW);
		postProcessSampleCount = std::max<GLsizei>(
			1, std::min<GLsizei>(msaaSamples, static_cast<GLsizei>(maxSampleCount)));
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
		glCreateFramebuffers(1, &postProcessDepthFramebuffer);
		glCreateTextures(GL_TEXTURE_2D, 1, &postProcessDepthTexture);
		glTextureStorage2D(postProcessDepthTexture, 1, GL_DEPTH_COMPONENT24, width, height);
		glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(postProcessDepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glNamedFramebufferTexture(postProcessDepthFramebuffer, GL_DEPTH_ATTACHMENT, postProcessDepthTexture, 0);
		glCreateTextures(GL_TEXTURE_2D, 1, &postProcessVelocityTexture);
		glTextureStorage2D(postProcessVelocityTexture, 1, GL_RG16F, width, height);
		glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(postProcessVelocityTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glNamedFramebufferTexture(postProcessDepthFramebuffer, GL_COLOR_ATTACHMENT0, postProcessVelocityTexture, 0);
		glNamedFramebufferDrawBuffer(postProcessDepthFramebuffer, GL_COLOR_ATTACHMENT0);
		glNamedFramebufferReadBuffer(postProcessDepthFramebuffer, GL_NONE);
		if (postProcessVao == 0)
			glCreateVertexArrays(1, &postProcessVao);
		return glCheckNamedFramebufferStatus(sceneFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(resolveFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(postProcessDepthFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& CreateEffectResources();
	}

	bool OpenGlPostProcess::Load(const std::vector<const EffectDefinition*>& effects) {
		OpenGlPostProcess candidate;
		candidate.targetWidth = targetWidth;
		candidate.targetHeight = targetHeight;
		if (!candidate.SetEffects(effects)
			|| (targetWidth > 0 && targetHeight > 0 && !candidate.CreateEffectResources()))
			return false;
		for (const auto& pass : candidate.ResolvePasses()) {
			auto shader = std::make_unique<OpenGlPostProcessShader>();
			if (!shader->Initialize(pass))
				return false;
			candidate.postProcessShaders.push_back(std::move(shader));
		}
		SwapExecutionPlan(candidate);
		resources.swap(candidate.resources);
		postProcessShaders.swap(candidate.postProcessShaders);
		ResetHistory();
		return true;
	}

	bool OpenGlPostProcess::BeginSceneInputPass(const int width, const int height) const {
		if (!RequiresDepth() && !RequiresVelocity())
			return false;
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
		return true;
	}

	void OpenGlPostProcess::EndSceneInputPass() {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	bool OpenGlPostProcess::Draw(
		const int width, const int height, const PostProcessFrameData& frameData) {
		if (!HasEffects())
			return true;
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
		const auto& routes = ResolvePassRoutes();
		for (size_t index = 0; index < postProcessShaders.size() && index < routes.size(); index++) {
			const PostProcessPassRoute& route = routes[index];
			glNamedBufferSubData(parameterDataBuffer, 0, sizeof(route.parameters), &route.parameters);
			glBindBufferBase(GL_UNIFORM_BUFFER,
				PostProcessInputLayout::parameterDataRegister, parameterDataBuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, ResolveOutputFramebuffer(route));
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			glViewport(0, 0, outputWidth, outputHeight);
			glUseProgram(postProcessShaders[index]->program);
			for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++)
				glBindTextureUnit(PostProcessInputLayout::ResolveSpirvTextureBinding(slot), sceneColorTexture);
			for (const auto& input : route.inputs)
				glBindTextureUnit(PostProcessInputLayout::ResolveSpirvTextureBinding(input.slot), ResolveInputTexture(input));
			glDrawArrays(GL_TRIANGLES, 0, 3);
			AdvanceHistory(route);
		}
		CommitHistoryFrame();
		return true;
	}

	void OpenGlPostProcess::ResetResources() {
		ResetShaders();
		ResetTargets();
		if (postProcessVao != 0)
			glDeleteVertexArrays(1, &postProcessVao);
		postProcessVao = 0;
	}
}
