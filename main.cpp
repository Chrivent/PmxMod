#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <shobjidl.h>

#include "AppContext.h"
#include "Model.h"
#include "base/Util.h"

#define MINIAUDIO_IMPLEMENTATION
#include <glm/fwd.hpp>

#include "external/miniaudio.h"

struct Input {
	std::filesystem::path				m_modelPath;
	std::vector<std::filesystem::path>	m_vmdPaths;
	float								m_scale = 1.0f;
};

inline bool PickFilesWin(
	std::vector<std::filesystem::path>& out,
	const wchar_t* title,
	const COMDLG_FILTERSPEC* filters,
	const UINT filterCount,
    const bool multi) {
    out.clear();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool coInited = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    IFileOpenDialog* dlg = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg)  {
    	if (coInited && hr != RPC_E_CHANGED_MODE)
    		CoUninitialize();
    	return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multi)
    	opts |= FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);
    if (title)
    	dlg->SetTitle(title);
    if (filters && filterCount)
    	dlg->SetFileTypes(filterCount, filters);
    hr = dlg->Show(nullptr);
    if (FAILED(hr)) {
    	if (dlg)
    		dlg->Release();
    	if (coInited && hr != RPC_E_CHANGED_MODE)
    		CoUninitialize();
    	return false;
    }
    if (multi) {
        IShellItemArray* items = nullptr;
        hr = dlg->GetResults(&items);
        if (SUCCEEDED(hr) && items) {
            DWORD n = 0;
            items->GetCount(&n);
            out.reserve(n);
            for (DWORD i = 0; i < n; ++i) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &item)) && item) {
                    PWSTR ws = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &ws)) && ws) {
                        out.emplace_back(ws);
                        CoTaskMemFree(ws);
                    }
                	if (item)
                		item->Release();
                }
            }
        }
    	if (items)
    		items->Release();
    } else {
        IShellItem* item = nullptr;
        hr = dlg->GetResult(&item);
        if (SUCCEEDED(hr) && item) {
            PWSTR ws = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &ws)) && ws) {
                out.emplace_back(ws);
                CoTaskMemFree(ws);
            }
        }
		if (item)
			item->Release();
    }
	if (dlg)
		dlg->Release();
    if (coInited && hr != RPC_E_CHANGED_MODE)
    	CoUninitialize();
    return !out.empty();
}

struct SceneConfig {
	std::vector<Input>		models;
	std::filesystem::path	cameraVmd;
	std::filesystem::path	musicPath;
};

static SceneConfig BuildTestSceneConfig() {
	SceneConfig cfg;
	Input in;
	in.m_modelPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\models\Chrivent Elf\Chrivent Elf.pmx)";
	in.m_scale = 1.0f;
	in.m_vmdPaths.emplace_back(R"(C:\Users\Ha Yechan\Desktop\PMXViewer\motions\(2)SukiYukiMajiMagic.vmd)");
	cfg.models.emplace_back(std::move(in));
	cfg.cameraVmd = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\cameras\(2)SukiYukiMajiMagic_camera.vmd)";
	cfg.musicPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\musics\02.wav)";
	Input bg;
	bg.m_modelPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\backgrounds\PDF 2nd Like, Dislike Stage\Like, Dislike Stage.pmx)";
	bg.m_scale = 1.0f;
	cfg.models.emplace_back(std::move(bg));
	return cfg;
}

static bool SampleMain(const SceneConfig& cfg) {
    ma_engine engine{};
    ma_sound  sound{};
    bool hasMusic = false;
    double prevTimeSec = 0.0;
    auto InitMusic = [&] {
        if (cfg.musicPath.empty())
        	return;
        if (ma_engine_init(nullptr, &engine) != MA_SUCCESS)
	        return;
        if (ma_sound_init_from_file(&engine,
        	reinterpret_cast<const char*>(cfg.musicPath.u8string().c_str()),
        	0, nullptr, nullptr, &sound) != MA_SUCCESS) {
            ma_engine_uninit(&engine);
            return;
        }
        ma_sound_start(&sound);
        hasMusic = true;
        prevTimeSec = 0.0;
    };
    auto UninitMusic = [&] {
        if (!hasMusic)
        	return;
        ma_sound_uninit(&sound);
        ma_engine_uninit(&engine);
        hasMusic = false;
    };
    auto PullMusicTimes = [&]() -> std::pair<float,float> {
        if (!hasMusic)
        	return { 0.f, 0.f };
        ma_uint64 frames{};
        if (ma_sound_get_cursor_in_pcm_frames(&sound, &frames) != MA_SUCCESS)
            return { 0.f, static_cast<float>(prevTimeSec) };
        const double sr = ma_engine_get_sample_rate(&engine);
        const double t  = sr > 0.0 ? static_cast<double>(frames) / sr : prevTimeSec;
        double dt = t - prevTimeSec;
    	if (dt < 0.0)
    		dt = 0.0;
        prevTimeSec = t;
        return { static_cast<float>(dt), static_cast<float>(t) };
    };
    InitMusic();
    if (!glfwInit()) {
	    UninitMusic();
    	return false;
    }
    AppContext appContext;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, appContext.m_msaaSamples);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
    if (!window) {
	    glfwTerminate();
    	UninitMusic();
    	return false;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwTerminate();
    	UninitMusic();
    	return false;
    }
    glfwSwapInterval(0);
    glEnable(GL_MULTISAMPLE);
    if (!appContext.Setup()) {
        std::cout << "Failed to setup AppContext.\n";
        glfwTerminate();
    	UninitMusic();
    	return false;
    }
    if (!cfg.cameraVmd.empty()) {
        VMDFile camVmd{};
        if (ReadVMDFile(&camVmd, cfg.cameraVmd.c_str()) && !camVmd.m_cameras.empty()) {
            auto vmdCamAnim = std::make_unique<VMDCameraAnimation>();
            if (!vmdCamAnim->Create(camVmd))
                std::cout << "Failed to create VMDCameraAnimation.\n";
            appContext.m_vmdCameraAnim = std::move(vmdCamAnim);
        }
    }
    std::vector<Model> models;
    models.reserve(cfg.models.size());
    for (const auto&[m_modelPath, m_vmdPaths, m_scale] : cfg.models) {
        Model model;
        const auto ext = PathUtil::GetExt(m_modelPath);
        if (ext != "pmx") {
            std::cout << "Unknown file type. [" << ext << "]\n";
            glfwTerminate();
        	UninitMusic();
        	return false;
        }
        auto pmxModel = std::make_unique<MMDModel>();
        if (!pmxModel->Load(m_modelPath, appContext.m_mmdDir)) {
            std::cout << "Failed to load pmx file.\n";
            glfwTerminate();
        	UninitMusic();
        	return false;
        }
        model.m_mmdModel = std::move(pmxModel);
        model.m_mmdModel->InitializeAnimation();
        auto vmdAnim = std::make_unique<VMDAnimation>();
        vmdAnim->m_model = model.m_mmdModel;
        for (const auto& vmdPath : m_vmdPaths) {
            VMDFile vmdFile{};
            if (!ReadVMDFile(&vmdFile, vmdPath.c_str())) {
                std::cout << "Failed to read VMD file.\n";
                glfwTerminate();
            	UninitMusic();
            	return false;
            }
            if (!vmdAnim->Add(vmdFile)) {
                std::cout << "Failed to add VMDAnimation.\n";
                glfwTerminate();
            	UninitMusic();
            	return false;
            }
        }
        vmdAnim->SyncPhysics(0.0f);
        model.m_vmdAnim = std::move(vmdAnim);
        model.m_scale = m_scale;
        model.Setup(appContext);
        models.emplace_back(std::move(model));
    }
    auto fpsTime  = std::chrono::steady_clock::now();
    auto saveTime = std::chrono::steady_clock::now();
    int fpsFrame  = 0;
    while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - saveTime).count();
        if (elapsed > 1.0 / 30.0)
        	elapsed = 1.0 / 30.0;
        saveTime = now;
        auto dt = static_cast<float>(elapsed);
        float t  = appContext.m_animTime + dt;
        if (hasMusic) {
            auto [adt, at] = PullMusicTimes();
            if (adt < 0.f)
            	adt = 0.f;
            dt = adt;
            t  = at;
        }
        appContext.m_elapsed  = dt;
        appContext.m_animTime = t;
        glClearColor(0.839f, 0.902f, 0.961f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        appContext.m_screenWidth  = width;
        appContext.m_screenHeight = height;
        glViewport(0, 0, width, height);
        if (appContext.m_vmdCameraAnim) {
            appContext.m_vmdCameraAnim->Evaluate(appContext.m_animTime * 30.0f);
            const auto mmdCam = appContext.m_vmdCameraAnim->m_camera;
            appContext.m_viewMat = mmdCam.GetViewMatrix();
            appContext.m_projMat = glm::perspectiveFovRH(mmdCam.m_fov,
                static_cast<float>(width), static_cast<float>(height), 1.0f, 10000.0f);
        } else {
            appContext.m_viewMat = glm::lookAt(glm::vec3(0,10,40), glm::vec3(0,10,0), glm::vec3(0,1,0));
            appContext.m_projMat = glm::perspectiveFovRH(glm::radians(30.0f),
                static_cast<float>(width), static_cast<float>(height), 1.0f, 10000.0f);
        }
        for (auto& model : models) {
            model.UpdateAnimation(appContext);
            model.Update();
            model.Draw(appContext);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
        fpsFrame++;
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
        if (sec > 1.0) {
            std::cout << (fpsFrame / sec) << " fps\n";
            fpsFrame = 0;
            fpsTime = std::chrono::steady_clock::now();
        }
    }
    appContext.Clear();
    glfwTerminate();
    UninitMusic();
    return true;
}

int main() {
	constexpr COMDLG_FILTERSPEC kModelFilters[]  = { {L"PMX Model", L"*.pmx"} };
	constexpr COMDLG_FILTERSPEC kVMDFilters[]    = { {L"VMD Motion/Camera", L"*.vmd"} };
	constexpr COMDLG_FILTERSPEC kMusicFilters[]  = { {L"Audio", L"*.wav;*.mp3;*.ogg;*.flac"} };
	const bool kTestMode = false;
	SceneConfig cfg;
	if (kTestMode)
		cfg = BuildTestSceneConfig();
	else {
		std::vector<std::vector<std::filesystem::path>> modelPaths;
		std::vector<std::vector<std::filesystem::path>> motionPaths;
		std::vector<std::filesystem::path> cameraPath;
		std::vector<std::filesystem::path> musicPath;
		std::vector<std::filesystem::path> bgPath;
		int n;
		std::cin >> n;
		modelPaths.resize(n);
		for (int i = 0; i < n; i++) {
			if (!PickFilesWin(modelPaths[i], L"모델(.pmx) 선택", kModelFilters, 1, true)) {
				std::cout << "모델 선택 취소\n";
				return 0;
			}
		}
		motionPaths.resize(n);
		for (int i = 0; i < n; i++)
			PickFilesWin(motionPaths[i], L"모션(.vmd) 선택 (취소=없음)", kVMDFilters, 1, true);
		PickFilesWin(cameraPath, L"카메라(.vmd) 선택 (취소=없음)", kVMDFilters, 1, false);
		PickFilesWin(musicPath, L"음악 선택 (취소=없음)", kMusicFilters, 1, false);
		PickFilesWin(bgPath, L"배경/스테이지 모델(.pmx) 선택 (취소=없음)", kModelFilters, 1, false);
		for (int i = 0; i < n; i++) {
			Input in;
			in.m_modelPath = modelPaths[i][0].wstring();
			in.m_scale = 1.0f;
			for (auto& v : motionPaths[i])
				in.m_vmdPaths.emplace_back(v.wstring());
			cfg.models.emplace_back(std::move(in));
		}
		if (!bgPath.empty()) {
			Input bg;
			bg.m_modelPath = bgPath.front().wstring();
			bg.m_scale = 1.0f;
			cfg.models.emplace_back(std::move(bg));
		}
		if (!cameraPath.empty())
			cfg.cameraVmd = cameraPath.front().wstring();
		if (!musicPath.empty())
			cfg.musicPath = musicPath.front().wstring();
	}
	if (!SampleMain(cfg)) {
		std::cout << "Failed to run.\n";
		return 1;
	}
	return 0;
}
