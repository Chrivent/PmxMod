#include "Viewer/Glfw/GlfwPostProcess.h"

#include "Viewer/Shader/PostProcessInputLayout.h"

#include <algorithm>
#include <filesystem>

namespace Chrivent {
	GlfwPostProcess::~GlfwPostProcess() {
		Reset();
	}

	void GlfwPostProcess::UpdateFocusHistory() {
		if (!focusHistoryEnabled || !focusHistoryShader)
			return;
		const int readIndex = focusHistoryIndex;
		const int writeIndex = 1 - focusHistoryIndex;
		glBindFramebuffer(GL_FRAMEBUFFER, focusHistoryFramebuffers[writeIndex]);
		glViewport(0, 0, 1, 1);
		glUseProgram(focusHistoryShader->program);
		glBindTextureUnit(PostProcessInputLayout::SceneColorRegister, sceneColorTexture);
		glBindTextureUnit(PostProcessInputLayout::SceneDepthRegister, postProcessDepthTexture);
		glBindTextureUnit(PostProcessInputLayout::FocusHistoryRegister, focusHistoryTextures[readIndex]);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		focusHistoryIndex = writeIndex;
	}

	void GlfwPostProcess::ResetTargets() {
		glDeleteTextures(2, focusHistoryTextures);
		glDeleteFramebuffers(2, focusHistoryFramebuffers);
		glDeleteTextures(2, pingPongTextures);
		glDeleteFramebuffers(2, pingPongFramebuffers);
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
		sceneDepthStencil = 0;
		sceneColorMsaa = 0;
		sceneColorTexture = 0;
		postProcessDepthTexture = 0;
		postProcessDepthFramebuffer = 0;
		resolveFramebuffer = 0;
		sceneFramebuffer = 0;
		pingPongTextures[0] = 0;
		pingPongTextures[1] = 0;
		pingPongFramebuffers[0] = 0;
		pingPongFramebuffers[1] = 0;
		focusHistoryTextures[0] = 0;
		focusHistoryTextures[1] = 0;
		focusHistoryFramebuffers[0] = 0;
		focusHistoryFramebuffers[1] = 0;
		postProcessSampleCount = 1;
		focusHistoryIndex = 0;
	}

	void GlfwPostProcess::ResetShaders() {
		ClearEffects();
		postProcessShaders.clear();
		focusHistoryShader.reset();
		focusHistoryEnabled = false;
		focusHistoryIndex = 0;
	}

	bool GlfwPostProcess::InitializeTargets(
		const int width, const int height, const int msaaSamples, const uint32_t maxSampleCount) {
		ResetTargets();
		if (width <= 0 || height <= 0)
			return false;
		postProcessSampleCount = std::max<GLsizei>(
			1, std::min<GLsizei>(msaaSamples, static_cast<GLsizei>(maxSampleCount)));
		glCreateFramebuffers(1, &sceneFramebuffer);
		glCreateRenderbuffers(1, &sceneColorMsaa);
		glCreateRenderbuffers(1, &sceneDepthStencil);
		if (postProcessSampleCount > 1) {
			glNamedRenderbufferStorageMultisample(sceneColorMsaa, postProcessSampleCount, GL_RGBA8, width, height);
			glNamedRenderbufferStorageMultisample(sceneDepthStencil, postProcessSampleCount, GL_DEPTH24_STENCIL8, width, height);
		} else {
			glNamedRenderbufferStorage(sceneColorMsaa, GL_RGBA8, width, height);
			glNamedRenderbufferStorage(sceneDepthStencil, GL_DEPTH24_STENCIL8, width, height);
		}
		glNamedFramebufferRenderbuffer(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColorMsaa);
		glNamedFramebufferRenderbuffer(sceneFramebuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthStencil);
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
		glCreateFramebuffers(2, pingPongFramebuffers);
		glCreateTextures(GL_TEXTURE_2D, 2, pingPongTextures);
		for (int index = 0; index < 2; index++) {
			glTextureStorage2D(pingPongTextures[index], 1, GL_RGBA8, width, height);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glNamedFramebufferTexture(pingPongFramebuffers[index], GL_COLOR_ATTACHMENT0, pingPongTextures[index], 0);
			if (glCheckNamedFramebufferStatus(pingPongFramebuffers[index], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				return false;
		}
		glCreateFramebuffers(2, focusHistoryFramebuffers);
		glCreateTextures(GL_TEXTURE_2D, 2, focusHistoryTextures);
		for (int index = 0; index < 2; index++) {
			glTextureStorage2D(focusHistoryTextures[index], 1, GL_RGBA32F, 1, 1);
			glTextureParameteri(focusHistoryTextures[index], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(focusHistoryTextures[index], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(focusHistoryTextures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(focusHistoryTextures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glNamedFramebufferTexture(focusHistoryFramebuffers[index], GL_COLOR_ATTACHMENT0, focusHistoryTextures[index], 0);
			constexpr float clearHistory[4] = {};
			glClearNamedFramebufferfv(focusHistoryFramebuffers[index], GL_COLOR, 0, clearHistory);
			if (glCheckNamedFramebufferStatus(focusHistoryFramebuffers[index], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				return false;
		}
		if (postProcessVao == 0)
			glCreateVertexArrays(1, &postProcessVao);
		return glCheckNamedFramebufferStatus(sceneFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(resolveFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(postProcessDepthFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	}

	bool GlfwPostProcess::Load(const std::vector<const EffectDefinition*>& effects) {
		ResetShaders();
		SetEffects(effects);
		for (const auto* effect : ResolveEffectPointers()) {
			auto shader = std::make_unique<GlfwPostProcessShader>();
			if (!shader->Initialize(effect->passes.front())) {
				ResetShaders();
				return false;
			}
			postProcessShaders.push_back(std::move(shader));
			if (effect->id == "depth-of-field") {
				EffectPassDefinition focusPass = effect->passes.front();
				focusPass.shaderPath = focusPass.shaderPath.parent_path() / "focus-update.hlsl";
				if (std::filesystem::exists(focusPass.shaderPath)) {
					focusHistoryShader = std::make_unique<GlfwPostProcessShader>();
					if (focusHistoryShader->Initialize(focusPass))
						focusHistoryEnabled = true;
					else
						focusHistoryShader.reset();
				}
			}
		}
		focusHistoryIndex = 0;
		return true;
	}

	bool GlfwPostProcess::BeginDepthPass(const int width, const int height) const {
		if (!HasEffects())
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

	void GlfwPostProcess::Draw(const int width, const int height) {
		if (!HasEffects())
			return;
		glBlitNamedFramebuffer(sceneFramebuffer, resolveFramebuffer,
			0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glBindVertexArray(postProcessVao);
		UpdateFocusHistory();
		GLuint sourceTexture = sceneColorTexture;
		for (size_t index = 0; index < postProcessShaders.size(); index++) {
			const bool lastPass = index + 1 == postProcessShaders.size();
			const size_t targetIndex = index % 2;
			glBindFramebuffer(GL_FRAMEBUFFER, lastPass ? 0 : pingPongFramebuffers[targetIndex]);
			glViewport(0, 0, width, height);
			glUseProgram(postProcessShaders[index]->program);
			glBindTextureUnit(PostProcessInputLayout::SceneColorRegister, sourceTexture);
			glBindTextureUnit(PostProcessInputLayout::SceneDepthRegister, postProcessDepthTexture);
			glBindTextureUnit(PostProcessInputLayout::FocusHistoryRegister,
				focusHistoryEnabled ? focusHistoryTextures[focusHistoryIndex] : postProcessDepthTexture);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			sourceTexture = pingPongTextures[targetIndex];
		}
	}

	void GlfwPostProcess::Reset() {
		ResetShaders();
		ResetTargets();
		if (postProcessVao != 0)
			glDeleteVertexArrays(1, &postProcessVao);
		postProcessVao = 0;
	}
}
