//#include <GL/gl3w.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>

#include "Path.h"

#include "AppContext.h"
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

#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

bool SampleMain(std::vector<std::string>& args)
{
	Input currentInput;
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
	}

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
			const auto mmdCam = appContext.m_vmdCameraAnim->m_camera;
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
			model.Update();

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

namespace fs = std::filesystem;

// ===== 하드코딩 경로 =====
static const fs::path MODEL_DIR  = "C:/Users/Ha Yechan/Desktop/PMXViewer/models";
static const fs::path MOTION_DIR = "C:/Users/Ha Yechan/Desktop/PMXViewer/motions";
static const fs::path CAMERA_DIR = "C:/Users/Ha Yechan/Desktop/PMXViewer/cameras";

// 소문자화 (ASCII 확장자 비교용)
static inline std::string tolower_copy(std::string s){
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

// --- 안전 출력(helper): u8string을 그대로 write ---
static inline void PrintU8(const std::u8string& s) {
    std::cout.write(reinterpret_cast<const char*>(s.c_str()),
                    static_cast<std::streamsize>(s.size()));
}
static inline void PrintPathSafe(const fs::path& p) { PrintU8(p.u8string()); }
static inline void PrintFilenameSafe(const fs::path& p) { PrintU8(p.filename().u8string()); }

// --- 경로 → UTF-8 std::string (args에 넣을 때 필수) ---
static inline std::string PathToUtf8(const fs::path& p){
    auto u8 = p.lexically_normal().u8string();                    // basic_string<char8_t>
    return std::string(reinterpret_cast<const char*>(u8.c_str()), // → std::string(UTF-8)
                       u8.size());
}

// 하위 폴더 나열
static std::vector<fs::path> ListSubdirs(const fs::path& root){
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return out;

    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)){
        if (ec) break;
        if (it->is_directory(ec)) {
            if (!it->path().filename().native().empty()) // 빈 이름 방지
                out.push_back(it->path());
        }
    }
    std::sort(out.begin(), out.end(),
        [](const fs::path& a, const fs::path& b){
            return a.filename().native() < b.filename().native();
        });
    return out;
}

// dir에서 특정 확장자만(.pmx/.vmd) 나열
static std::vector<fs::path> ListFilesWithExt(const fs::path& dir, const char* wantExtLower){
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return out;

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)){
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;

        std::string ext = tolower_copy(it->path().extension().string());
        if (ext != wantExtLower) continue;

        if (it->path().filename().native().empty()) continue; // 빈 파일명 방지
        out.push_back(it->path());
    }
    std::sort(out.begin(), out.end(),
        [](const fs::path& a, const fs::path& b){
            return a.filename().native() < b.filename().native();
        });
    return out;
}

static void PrintIndexed(const std::vector<fs::path>& items, const char* title){
    std::cout << title << "\n";
    for (size_t i=0;i<items.size();++i){
        std::cout << "  [" << i << "] ";
        PrintFilenameSafe(items[i]);    // 안전 출력
        std::cout << "\n";
    }
}

static std::vector<int> ReadMultiIndex(const std::string& prompt, size_t maxN){
    std::cout << prompt;
    std::string line; std::getline(std::cin, line);
    std::istringstream iss(line);
    std::vector<int> idxs; int v;
    while (iss >> v) if (v>=0 && (size_t)v<maxN) idxs.push_back(v);
    std::sort(idxs.begin(), idxs.end());
    idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
    return idxs;
}

static int ReadOneIndex(const std::string& prompt, size_t maxN){
    std::cout << prompt;
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return -1;
    try { int v = std::stoi(s); if (v>=0 && (size_t)v<maxN) return v; } catch(...) {}
    return -1;
}

static bool YesNo(const std::string& prompt, bool defNo=true){
    std::cout << prompt;
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return !defNo;
    char c = (char)std::tolower(s[0]);
    return (c=='y' || c=='1' || c=='t');
}

// === 핵심: 간단/짧은 인터랙티브 args 빌더 ===
static std::vector<std::string> BuildArgsInteractive(){
    std::vector<std::string> args;

    auto modelFolders = ListSubdirs(MODEL_DIR);
    auto motions      = ListFilesWithExt(MOTION_DIR, ".vmd");
    auto cameras      = ListFilesWithExt(CAMERA_DIR, ".vmd");

    if (modelFolders.empty()){
        std::cout << "[오류] 모델 루트 폴더가 비었어요: ";
        PrintPathSafe(MODEL_DIR); std::cout << "\n";
        return args;
    }

    std::cout << "\n[모델 폴더]\n";
    PrintIndexed(modelFolders, "");
    auto modelSel = ReadMultiIndex("추가할 모델 폴더 번호들(공백 구분, 비면 취소): ", modelFolders.size());
    if (modelSel.empty()){
        std::cout << "[오류] 모델을 선택하지 않았습니다.\n";
        return args;
    }

    if (!motions.empty()){
        PrintIndexed(motions, "\n[모션 VMD]");
    }
    auto motionSel = motions.empty() ? std::vector<int>{}
                                     : ReadMultiIndex("이 모델들에 붙일 모션 번호들(공백, 비면 없음): ", motions.size());

    // 각 모델 폴더에서 PMX 하나 자동 선택
    for (int fIdx : modelSel){
        const fs::path& folder = modelFolders[(size_t)fIdx];
        auto pmxs = ListFilesWithExt(folder, ".pmx");
        if (pmxs.empty()){
            std::cout << "  [건너뜀] ";
            PrintFilenameSafe(folder);
            std::cout << " 폴더에 .pmx 없음.\n";
            continue;
        }
        // 폴더에 하나만 있다고 했으니 첫 번째 사용
        args.push_back("-model");
        args.push_back(PathToUtf8(pmxs[0]));  // ← 여기!

        for (int mi : motionSel){
            args.push_back("-vmd");
            args.push_back(PathToUtf8(motions[(size_t)mi])); // ← 여기!
        }
    }

    // 카메라(선택, 0~1)
    if (!cameras.empty()){
        PrintIndexed(cameras, "\n[카메라 VMD]");
        int cIdx = ReadOneIndex("카메라 번호(엔터=없음): ", cameras.size());
        if (cIdx >= 0){
            args.push_back("-vmd"); // 마지막에 추가 → 전역 카메라로 적용
            args.push_back(PathToUtf8(cameras[(size_t)cIdx])); // ← 여기!
        }
    }

    if (YesNo("\n투명창 모드 사용? (y/N): ", true)){
        args.push_back("-transparent");
    }

    std::cout << "\n[최종 인자]\n";
    for (auto& s : args) std::cout << s << ' ';
    std::cout << "\n";
    return args;
}

int main()
{
	auto args = BuildArgsInteractive();

	if (!SampleMain(args))
	{
		std::cout << "Failed to run.\n";
		return 1;
	}
	return 0;
}
