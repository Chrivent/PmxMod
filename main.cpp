//#include <GL/gl3w.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define  GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define	STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#include "Path.h"
#include "File.h"
#include "VMDFile.h"
#include "VMDAnimation.h"
#include "VMDCameraAnimation.h"

#include "AppContext.h"
#include "MMDShader.h"
#include "Model.h"

struct Input
{
	std::string					m_modelPath;
	std::vector<std::string>	m_vmdPaths;
};

void Usage()
{
	std::cout << "app [-model <pmd|pmx file path>] [-vmd <vmd file path>]\n";
	std::cout << "e.g. app -model model1.pmx -vmd anim1_1.vmd -vmd anim1_2.vmd  -model model2.pmx\n";
}

#include <io.h>
#include <fcntl.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

// 폴더에서 .pmx/.pmd 찾기
static std::vector<std::wstring> FindAllPMDPMX(const std::wstring& root) {
	std::vector<std::wstring> out;
	if (!std::filesystem::exists(root)) return out;
	for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
		if (!e.is_regular_file()) continue;
		auto ext = e.path().extension().wstring();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
		if (ext == L".pmx" || ext == L".pmd") out.push_back(e.path().wstring());
	}
	return out;
}

// 폴더에서 .vmd 찾기
static std::vector<std::wstring> FindAllVMD(const std::wstring& root) {
	std::vector<std::wstring> out;
	if (!std::filesystem::exists(root)) return out;
	for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
		if (!e.is_regular_file()) continue;
		if (e.path().extension() == L".vmd") out.push_back(e.path().wstring());
	}
	return out;
}

static std::string WUtf8(const std::wstring& ws) {
	if (ws.empty()) return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), s.data(), n, nullptr, nullptr);
	return s;
}

bool SampleMain()
{
	_setmode(_fileno(stdin), _O_TEXT);
	_setmode(_fileno(stdout), _O_TEXT);
	_setmode(_fileno(stderr), _O_TEXT);

	Input currentInput;
	std::vector<Input> inputModels;
	bool enableTransparentWindow = false;

	// ---- 모델 폴더 입력 & 선택 ----
	std::wstring modelRoot = L"C:/Users/Ha Yechan/Desktop/PMXViewer/models";
	auto modelsFound = FindAllPMDPMX(modelRoot);
	if (modelsFound.empty()) {
		std::cerr << "[오류] PMX/PMD가 없습니다: " << WUtf8(modelRoot) << "\n";
		return false;
	}
	std::cout << "\n[모델 목록]\n";
	for (size_t i = 0; i < modelsFound.size(); ++i)
		std::cout << i << ": " << WUtf8(modelsFound[i]) << "\n";
	std::cout << "\n불러올 모델 번호: ";
	int selModel = -1; std::cin >> selModel; std::cin.ignore(1024, '\n');
	if (selModel < 0 || selModel >= (int)modelsFound.size()) {
		std::cerr << "잘못된 선택입니다.\n"; return false;
	}
	currentInput.m_modelPath = WUtf8(modelsFound[selModel]);

	// ---- 모션 폴더 입력 & 선택 ----
	std::wstring motionRoot = L"C:/Users/Ha Yechan/Desktop/PMXViewer/motions";
	auto motionsFound = FindAllVMD(motionRoot);
	if (motionsFound.empty()) {
		std::cerr << "[오류] VMD가 없습니다: " << WUtf8(motionRoot) << "\n";
		return false;
	}
	std::cout << "\n[모션 목록]\n";
	for (size_t i = 0; i < motionsFound.size(); ++i)
		std::cout << i << ": " << WUtf8(motionsFound[i]) << "\n";
	std::cout << "\n불러올 VMD 번호: ";
	int selVmd = -1; std::cin >> selVmd; std::cin.ignore(1024, '\n');
	if (selVmd < 0 || selVmd >= (int)motionsFound.size()) {
		std::cerr << "잘못된 선택입니다.\n"; return false;
	}
	currentInput.m_vmdPaths.push_back(WUtf8(motionsFound[selVmd]));

	// ---- 투명 창 여부 ----
	std::cout << "\n투명 창 사용? (y/N): ";
	std::string yn; std::getline(std::cin, yn);
	enableTransparentWindow = (!yn.empty() && (yn[0] == 'y' || yn[0] == 'Y'));

	inputModels.emplace_back(currentInput);

	/*Input currentInput;
	std::vector<Input> inputModels;
	bool enableTransparentWindow = false;
	for (auto argIt = args.begin(); argIt != args.end(); ++argIt)
	{
		const auto& arg = (*argIt);
		if (arg == "-model")
		{
			if (!currentInput.m_modelPath.empty())
			{
				inputModels.emplace_back(currentInput);
			}

			++argIt;
			if (argIt == args.end())
			{
				Usage();
				return false;
			}
			currentInput = Input();
			currentInput.m_modelPath = (*argIt);
		}
		else if (arg == "-vmd")
		{
			if (currentInput.m_modelPath.empty())
			{
				Usage();
				return false;
			}

			++argIt;
			if (argIt == args.end())
			{
				Usage();
				return false;
			}
			currentInput.m_vmdPaths.push_back((*argIt));
		}
		else if (arg == "-transparent")
		{
			enableTransparentWindow = true;
		}
	}
	if (!currentInput.m_modelPath.empty())
	{
		inputModels.emplace_back(currentInput);
	}*/

	// Initialize glfw
	if (!glfwInit())
	{
		return false;
	}
	AppContext appContext;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, appContext.m_msaaSamples);
#if defined(GLFW_TRANSPARENT_FRAMEBUFFER)
	if (enableTransparentWindow)
	{
		glfwWindowHint(GLFW_SAMPLES, 0);
		glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
	}
#endif // defined(GLFW_TRANSPARENT_FRAMEBUFFER)

	auto window = glfwCreateWindow(1280, 800, "Pmx Mod", nullptr, nullptr);
	if (window == nullptr)
	{
		return false;
	}

#if _WIN32 && (GLFW_VERSION_MAJOR >= 3) && (GLFW_VERSION_MINOR >= 3) && (GLFW_VERSION_REVISION >= 3)
	// The color key was removed from glfw3.3.3. (Windows)
	if (enableTransparentWindow)
	{
		HWND hwnd = glfwGetWin32Window(window);
		LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
		exStyle |= WS_EX_LAYERED;
		SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);
		SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 255, LWA_COLORKEY);
	}
#endif // _WIN32

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		return false;
	}

	/*if (gl3wInit() != 0)
	{
		return false;
	}*/

	glfwSwapInterval(0);
	glEnable(GL_MULTISAMPLE);

	// Initialize application
	if (!appContext.Setup())
	{
		std::cout << "Failed to setup AppContext.\n";
		return false;
	}

	appContext.m_enableTransparentWindow = enableTransparentWindow;

	// Load MMD model
	std::vector<Model> models;
	for (const auto& input : inputModels)
	{
		// Load MMD model
		Model model;
		auto ext = saba::PathUtil::GetExt(input.m_modelPath);
		if (ext == "pmx")
		{
			auto pmxModel = std::make_unique<saba::MMDModel>();
			if (!pmxModel->Load(input.m_modelPath, appContext.m_mmdDir))
			{
				std::cout << "Failed to load pmx file.\n";
				return false;
			}
			model.m_mmdModel = std::move(pmxModel);
		}
		else
		{
			std::cout << "Unknown file type. [" << ext << "]\n";
			return false;
		}
		model.m_mmdModel->InitializeAnimation();

		// Load VMD animation
		auto vmdAnim = std::make_unique<saba::VMDAnimation>();
		vmdAnim->m_model = model.m_mmdModel;
		for (const auto& vmdPath : input.m_vmdPaths)
		{
			saba::VMDFile vmdFile;
			if (!saba::ReadVMDFile(&vmdFile, vmdPath.c_str()))
			{
				std::cout << "Failed to read VMD file.\n";
				return false;
			}
			if (!vmdAnim->Add(vmdFile))
			{
				std::cout << "Failed to add VMDAnimation.\n";
				return false;
			}
			if (!vmdFile.m_cameras.empty())
			{
				auto vmdCamAnim = std::make_unique<saba::VMDCameraAnimation>();
				if (!vmdCamAnim->Create(vmdFile))
				{
					std::cout << "Failed to create VMDCameraAnimation.\n";
				}
				appContext.m_vmdCameraAnim = std::move(vmdCamAnim);
			}
		}
		vmdAnim->SyncPhysics(0.0f);

		model.m_vmdAnim = std::move(vmdAnim);

		model.Setup(appContext);

		models.emplace_back(std::move(model));
	}

	auto fpsTime = std::chrono::steady_clock::now();
	int fpsFrame = 0;
	auto saveTime = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window))
	{
		auto time = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(time - saveTime).count();
		if (elapsed > 1.0 / 30.0)
		{
			elapsed = 1.0 / 30.0;
		}
		saveTime = time;
		appContext.m_elapsed = float(elapsed);
		appContext.m_animTime += float(elapsed);

		if (enableTransparentWindow)
		{
			appContext.SetupTransparentFBO();
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}
		else
		{
			glClearColor(1.0f, 0.8f, 0.75f, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		appContext.m_screenWidth = width;
		appContext.m_screenHeight = height;

		glViewport(0, 0, width, height);

		// Setup camera
		if (appContext.m_vmdCameraAnim)
		{
			appContext.m_vmdCameraAnim->Evaluate(appContext.m_animTime * 30.0f);
			const auto mmdCam = appContext.m_vmdCameraAnim->GetCamera();
			saba::MMDLookAtCamera lookAtCam(mmdCam);
			appContext.m_viewMat = glm::lookAt(lookAtCam.m_eye, lookAtCam.m_center, lookAtCam.m_up);
			appContext.m_projMat = glm::perspectiveFovRH(mmdCam.m_fov, float(width), float(height), 1.0f, 10000.0f);
		}
		else
		{
			appContext.m_viewMat = glm::lookAt(glm::vec3(0, 10, 50), glm::vec3(0, 10, 0), glm::vec3(0, 1, 0));
			appContext.m_projMat = glm::perspectiveFovRH(glm::radians(30.0f), float(width), float(height), 1.0f, 10000.0f);
		}

		for (auto& model : models)
		{
			// Update Animation
			model.UpdateAnimation(appContext);

			// Update Vertices
			model.Update(appContext);

			// Draw
			model.Draw(appContext);
		}

		if (enableTransparentWindow)
		{
			glDisable(GL_MULTISAMPLE);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, appContext.m_transparentFbo);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, appContext.m_transparentMSAAFbo);
			glDrawBuffer(GL_BACK);
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

			glDisable(GL_DEPTH_TEST);
			glBindVertexArray(appContext.m_copyVAO);
			glUseProgram(appContext.m_copyTransparentWindowShader);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, appContext.m_transparentFboColorTex);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glBindVertexArray(0);
			glUseProgram(0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();

		//FPS
		{
			fpsFrame++;
			auto time = std::chrono::steady_clock::now();
			double deltaTime = std::chrono::duration<double>(time - fpsTime).count();
			if (deltaTime > 1.0)
			{
				double fps = double(fpsFrame) / deltaTime;
				std::cout << fps << " fps\n";
				fpsFrame = 0;
				fpsTime = time;
			}
		}
	}

	appContext.Clear();

	glfwTerminate();

	return true;
}

#include <Windows.h>
#include <shellapi.h>

int main()
{
	if (!SampleMain()) {
		std::cout << "Failed to run.\n";
		system("pause");
		return 1;
	}
	return 0;
}
