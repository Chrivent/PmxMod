#include "Viewer/Glfw/GlfwPostProcess.h"

#include "Viewer/Shader/PostProcessInputLayout.h"
#include "Viewer/Viewer.h"

#include <algorithm>

namespace Chrivent {
	GlfwPostProcess::~GlfwPostProcess() {
		GlfwPostProcess::Reset();
	}

	bool GlfwPostProcess::CreateEffectResources() {
		ResetEffectResources();
		const auto& plans = ResolveResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			GlfwPostProcessResource& resource = resources[resourceIndex];
			const GLsizei textureCount = plan.lifetime == EffectResourceLifetime::History ? 2 : 1;
			const int width = ResolveResourceExtent(targetWidth, plan, true);
			const int height = ResolveResourceExtent(targetHeight, plan, false);
			const GLenum format = plan.format == EffectTextureFormat::Rgba8Unorm
				? GL_RGBA8 : plan.format == EffectTextureFormat::Rgba16Float ? GL_RGBA16F : GL_RGBA32F;
			glCreateFramebuffers(textureCount, resource.framebuffers);
			glCreateTextures(GL_TEXTURE_2D, textureCount, resource.textures);
			for (GLsizei index = 0; index < textureCount; index++) {
				glTextureStorage2D(resource.textures[index], 1, format, width, height);
				glTextureParameteri(resource.textures[index], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTextureParameteri(resource.textures[index], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTextureParameteri(resource.textures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(resource.textures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glNamedFramebufferTexture(
					resource.framebuffers[index], GL_COLOR_ATTACHMENT0, resource.textures[index], 0);
				if (glCheckNamedFramebufferStatus(resource.framebuffers[index], GL_FRAMEBUFFER)
					!= GL_FRAMEBUFFER_COMPLETE)
					return false;
			}
		}
		ResetHistory();
		return true;
	}

	void GlfwPostProcess::InitializeHistories() {
		const auto& plans = ResolveResourcePlans();
		constexpr float clearColor[4]{};
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			GlfwPostProcessResource& resource = resources[index];
			if (plans[index].lifetime != EffectResourceLifetime::History || resource.historyInitialized)
				continue;
			glClearNamedFramebufferfv(resource.framebuffers[0], GL_COLOR, 0, clearColor);
			glClearNamedFramebufferfv(resource.framebuffers[1], GL_COLOR, 0, clearColor);
			resource.historyIndex = 0;
			resource.historyInitialized = true;
		}
	}

	GLuint GlfwPostProcess::ResolveInputTexture(const PostProcessPassInputRoute& input) const {
		if (input.kind == PostProcessInputKind::SceneColor)
			return sceneColorTexture;
		if (input.kind == PostProcessInputKind::SceneDepth)
			return postProcessDepthTexture;
		if (input.resourceIndex >= resources.size())
			return sceneColorTexture;
		const auto& plans = ResolveResourcePlans();
		const GlfwPostProcessResource& resource = resources[input.resourceIndex];
		return plans[input.resourceIndex].lifetime == EffectResourceLifetime::History
			? resource.textures[resource.historyIndex] : resource.textures[0];
	}

	GLuint GlfwPostProcess::ResolveOutputFramebuffer(const PostProcessPassRoute& route) const {
		if (route.outputKind == PostProcessOutputKind::Present)
			return 0;
		if (route.outputResourceIndex >= resources.size())
			return 0;
		const auto& plans = ResolveResourcePlans();
		const GlfwPostProcessResource& resource = resources[route.outputResourceIndex];
		const size_t index = plans[route.outputResourceIndex].lifetime == EffectResourceLifetime::History
			? ResolveNextHistoryIndex(resource.historyIndex) : 0;
		return resource.framebuffers[index];
	}

	void GlfwPostProcess::ResolveOutputExtent(
		const PostProcessPassRoute& route, int& width, int& height) const {
		width = targetWidth;
		height = targetHeight;
		if (route.outputKind == PostProcessOutputKind::Present
			|| route.outputResourceIndex >= ResolveResourcePlans().size())
			return;
		const PostProcessResourcePlan& plan = ResolveResourcePlans()[route.outputResourceIndex];
		width = ResolveResourceExtent(targetWidth, plan, true);
		height = ResolveResourceExtent(targetHeight, plan, false);
	}

	void GlfwPostProcess::AdvanceHistory(const PostProcessPassRoute& route) {
		if (route.outputKind != PostProcessOutputKind::Resource
			|| route.outputResourceIndex >= resources.size()
			|| ResolveResourcePlans()[route.outputResourceIndex].lifetime != EffectResourceLifetime::History)
			return;
		GlfwPostProcessResource& resource = resources[route.outputResourceIndex];
		resource.historyIndex = ResolveNextHistoryIndex(resource.historyIndex);
		resource.historyInitialized = true;
	}

	void GlfwPostProcess::ResetEffectResources() {
		const auto& plans = ResolveResourcePlans();
		for (size_t index = 0; index < resources.size(); index++) {
			const GLsizei count = index < plans.size() && plans[index].lifetime == EffectResourceLifetime::History ? 2 : 1;
			glDeleteTextures(count, resources[index].textures);
			glDeleteFramebuffers(count, resources[index].framebuffers);
		}
		resources.clear();
	}

	void GlfwPostProcess::ResetTargets() {
		ResetEffectResources();
		if (sceneDepthStencil != 0)
			glDeleteRenderbuffers(1, &sceneDepthStencil);
		if (sceneColorMsaa != 0)
			glDeleteRenderbuffers(1, &sceneColorMsaa);
		if (sceneColorTexture != 0)
			glDeleteTextures(1, &sceneColorTexture);
		if (postProcessDepthTexture != 0)
			glDeleteTextures(1, &postProcessDepthTexture);
		if (postProcessDepthFramebuffer != 0)
			glDeleteFramebuffers(1, &postProcessDepthFramebuffer);
		if (resolveFramebuffer != 0)
			glDeleteFramebuffers(1, &resolveFramebuffer);
		if (sceneFramebuffer != 0)
			glDeleteFramebuffers(1, &sceneFramebuffer);
		if (frameDataBuffer != 0)
			glDeleteBuffers(1, &frameDataBuffer);
		sceneDepthStencil = 0;
		sceneColorMsaa = 0;
		sceneColorTexture = 0;
		postProcessDepthTexture = 0;
		postProcessDepthFramebuffer = 0;
		resolveFramebuffer = 0;
		sceneFramebuffer = 0;
		frameDataBuffer = 0;
		postProcessSampleCount = 1;
		targetWidth = 0;
		targetHeight = 0;
	}

	void GlfwPostProcess::ResetShaders() {
		postProcessShaders.clear();
		ResetHistory();
	}

	bool GlfwPostProcess::InitializeTargets(
		const int width, const int height, const int msaaSamples, const uint32_t maxSampleCount) {
		ResetTargets();
		if (width <= 0 || height <= 0)
			return false;
		targetWidth = width;
		targetHeight = height;
		glCreateBuffers(1, &frameDataBuffer);
		if (frameDataBuffer == 0)
			return false;
		glNamedBufferData(frameDataBuffer, sizeof(PostProcessFrameData), nullptr, GL_DYNAMIC_DRAW);
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
		glNamedFramebufferDrawBuffer(postProcessDepthFramebuffer, GL_NONE);
		glNamedFramebufferReadBuffer(postProcessDepthFramebuffer, GL_NONE);
		if (postProcessVao == 0)
			glCreateVertexArrays(1, &postProcessVao);
		return glCheckNamedFramebufferStatus(sceneFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(resolveFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(postProcessDepthFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& CreateEffectResources();
	}

	bool GlfwPostProcess::Load(const std::vector<const EffectDefinition*>& effects) {
		ResetShaders();
		ResetEffectResources();
		if (!SetEffects(effects) || (targetWidth > 0 && targetHeight > 0 && !CreateEffectResources())) {
			ClearShaders();
			return false;
		}
		for (const auto& pass : ResolvePasses()) {
			auto shader = std::make_unique<GlfwPostProcessShader>();
			if (!shader->Initialize(pass)) {
				ClearShaders();
				return false;
			}
			postProcessShaders.push_back(std::move(shader));
		}
		ResetHistory();
		return true;
	}

	void GlfwPostProcess::ClearShaders() {
		ResetShaders();
		ResetEffectResources();
		ClearEffects();
	}

	bool GlfwPostProcess::BeginDepthPass(const int width, const int height) const {
		if (!RequiresDepth())
			return false;
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessDepthFramebuffer);
		glViewport(0, 0, width, height);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_POLYGON_OFFSET_FILL);
		return true;
	}

	void GlfwPostProcess::EndDepthPass() {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GlfwPostProcess::Draw(
		const int width, const int height, const PostProcessFrameData& frameData) {
		if (!HasEffects())
			return;
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
	}

	void GlfwPostProcess::ResetHistory() {
		for (auto& resource : resources) {
			resource.historyIndex = 0;
			resource.historyInitialized = false;
		}
	}

	void GlfwPostProcess::Reset() {
		ResetShaders();
		ResetTargets();
		ClearEffects();
		if (postProcessVao != 0)
			glDeleteVertexArrays(1, &postProcessVao);
		postProcessVao = 0;
	}
}
