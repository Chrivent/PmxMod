#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

#include "base/Path.h"

#include "AppContext.h"
#include "Model.h"

#define MINIAUDIO_IMPLEMENTATION
#include "external/miniaudio.h"

namespace fs = std::filesystem;

static const std::string MODEL_DIR  = PathUtil::Normalize(R"(C:/Users/Ha Yechan/Desktop/PMXViewer/models)");
static const std::string MOTION_DIR = PathUtil::Normalize(R"(C:/Users/Ha Yechan/Desktop/PMXViewer/motions)");
static const std::string CAMERA_DIR = PathUtil::Normalize(R"(C:/Users/Ha Yechan/Desktop/PMXViewer/cameras)");
static const std::string MUSIC_DIR  = PathUtil::Normalize(R"(C:/Users/Ha Yechan/Desktop/PMXViewer/musics)");

static std::string Norm(const fs::path& p) {
    return PathUtil::Normalize(ToUtf8String(p.wstring()));
}

static void PrintPath(const std::string& u8) { std::cout << u8; }
static void PrintFilename(const std::string& u8path) {
    std::cout << PathUtil::GetFilename(u8path);
}

static std::vector<std::string> ListSubdirs(const std::string& rootU8){
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path root(rootU8);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return out;

    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) out.emplace_back(Norm(it->path()));
    }
    std::ranges::sort(out);
    return out;
}

static std::vector<std::string> ListFilesWithExt(const std::string& dirU8, std::string_view wantExtLower){
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path dir(dirU8);
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return out;

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        const std::string pathU8 = Norm(it->path());
        if (PathUtil::GetExt(pathU8) == wantExtLower) out.emplace_back(pathU8);
    }
    std::ranges::sort(out);
    return out;
}

static void PrintIndexed(const std::vector<std::string>& items, const char* title){
    std::cout << title << "\n";
    for (size_t i=0;i<items.size();++i){
        std::cout << "  [" << i << "] ";
        PrintFilename(items[i]);
        std::cout << "\n";
    }
}

static std::vector<int> ReadMultiIndex(const std::string& prompt, size_t maxN){
    std::cout << prompt;
    std::string line; std::getline(std::cin, line);
    std::istringstream iss(line);
    std::vector<int> idx; int v;
    while (iss >> v) if (v>=0 && static_cast<size_t>(v)<maxN) idx.push_back(v);
    std::ranges::sort(idx);
    idx.erase(std::ranges::unique(idx).begin(), idx.end());
    return idx;
}

static int ReadOneIndex(const std::string& prompt, size_t maxN){
    std::cout << prompt;
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return -1;
    try { const int v = std::stoi(s); if (v>=0 && static_cast<size_t>(v)<maxN) return v; } catch(...) {}
    return -1;
}

static std::vector<std::string> BuildArgsInteractive(){
    std::vector<std::string> args;

    const auto modelFolders = ListSubdirs(MODEL_DIR);
    const auto motions      = ListFilesWithExt(MOTION_DIR, "vmd");
    const auto cameras      = ListFilesWithExt(CAMERA_DIR, "vmd");

    if (modelFolders.empty()){
        std::cout << "[오류] 모델 루트 폴더가 비었어요: ";
        PrintPath(MODEL_DIR); std::cout << "\n";
        return args;
    }

    std::cout << "\n[모델 폴더]\n";
    PrintIndexed(modelFolders, "");
    const auto modelSel = ReadMultiIndex("추가할 모델 폴더 번호들(공백 구분, 비면 취소): ", modelFolders.size());
    if (modelSel.empty()){
        std::cout << "[오류] 모델을 선택하지 않았습니다.\n";
        return args;
    }

    if (!motions.empty()) PrintIndexed(motions, "\n[모션 VMD]");
    const auto motionSel = motions.empty() ? std::vector<int>{}
                                           : ReadMultiIndex("이 모델들에 붙일 모션 번호들(공백, 비면 없음): ", motions.size());

    for (const int fIdx : modelSel){
        const std::string& folder = modelFolders[(size_t)fIdx];
        const auto pmxs = ListFilesWithExt(folder, "pmx");
        if (pmxs.empty()){
            std::cout << "  [건너뜀] " << PathUtil::GetFilename(folder) << " 폴더에 .pmx 없음.\n";
            continue;
        }

        args.emplace_back("-model");
        args.emplace_back(pmxs[0]); // UTF-8 Path

        for (const int mi : motionSel){
            args.emplace_back("-vmd");
            args.emplace_back(motions[(size_t)mi]);
        }
    }

    // 카메라(선택, 0~1)
    if (!cameras.empty()){
        PrintIndexed(cameras, "\n[카메라 VMD]");
        const int cIdx = ReadOneIndex("카메라 번호(엔터=없음): ", cameras.size());
        if (cIdx >= 0){
            args.emplace_back("-vmd");             // 마지막 추가 → 전역 카메라
            args.emplace_back(cameras[(size_t)cIdx]);
        }
    }

    // 데모용 기본 추가
    args.emplace_back("-scale");
    args.emplace_back("1.0");
    args.emplace_back("-model");
    args.emplace_back(PathUtil::Normalize(R"(C:\Users\Ha Yechan\Desktop\PMXViewer\models\torisutsuki\torisutsuki.pmx)"));

    std::cout << "\n[최종 인자]\n";
    for (auto& s : args) std::cout << s << ' ';
    std::cout << "\n";
    return args;
}


static bool IsAudioExt(std::string_view extNoDotLower) {
    // PathUtil::GetExt는 점(.) 없이 소문자로 반환하므로 이렇게 비교
    static constexpr std::string_view exts[] = { "wav", "mp3", "ogg", "flac" };
    return std::ranges::any_of(exts, [&](auto e){ return extNoDotLower == e; });
}

// fs::path -> UTF-8 문자열로 정규화 (이미 위에 있는 Norm 사용)
static std::vector<std::string> ListAudioFiles(const std::string& dirU8) {
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path dir(dirU8);
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return out;

    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec))
    {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;

        const std::string pathU8 = Norm(it->path());
        if (IsAudioExt(PathUtil::GetExt(pathU8)))  // ← 점 없이 소문자
            out.emplace_back(pathU8);
    }

    std::ranges::sort(out);
    return out;
}

struct AudioChoice { std::string path; bool sync = false; };

static AudioChoice ChooseMusicInteractive() {
    AudioChoice choice;
    const auto musics = ListAudioFiles(MUSIC_DIR);
    if (musics.empty()) {
        std::cout << "\n[알림] 음악 폴더가 비었어요: "; PrintPath(MUSIC_DIR); std::cout << "\n";
        return choice;
    }

    PrintIndexed(musics, "\n[음악 파일]");
    const int idx = ReadOneIndex("재생할 음악 번호(엔터=없음): ", musics.size());
    if (idx >= 0) { choice.path = musics[(size_t)idx]; choice.sync = true; }
    return choice;
}

struct AudioContext {
    ma_engine engine{}; ma_sound sound{};
    bool hasMusic = false, syncToMusic = false;
    double prevTimeSec = 0.0;

    bool Init(const AudioChoice& c) {
        if (c.path.empty()) return false;
        if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
            std::cout << "[오디오] engine init 실패\n"; return false;
        }

        // c.path는 이미 UTF-8 문자열
        if (ma_sound_init_from_file(&engine, c.path.c_str(), 0, nullptr, nullptr, &sound) != MA_SUCCESS) {
            std::cout << "[오디오] 파일 열기 실패: " << PathUtil::GetFilename(c.path) << "\n";
            ma_engine_uninit(&engine); return false;
        }

        // ma_sound_set_looping(&sound, MA_TRUE); // 필요 시
        ma_sound_start(&sound);

        hasMusic    = true;
        syncToMusic = c.sync;
        prevTimeSec = 0.0;

        std::cout << "[오디오] 재생 시작: ";
        PrintFilename(c.path);
        std::cout << (syncToMusic ? " (싱크 ON)\n" : " (싱크 OFF)\n");
        return true;
    }

    void Uninit() noexcept {
        if (!hasMusic) return;
        ma_sound_uninit(&sound);
        ma_engine_uninit(&engine);
        hasMusic = false;
    }

    // (dt, t) seconds
    std::pair<float,float> PullTimes() {
        if (!hasMusic) return {0.f, 0.f};

        ma_uint64 frames{};
        if (ma_sound_get_cursor_in_pcm_frames(&sound, &frames) != MA_SUCCESS)
            return {0.f, (float)prevTimeSec};

        const double sr = (double)ma_engine_get_sample_rate(&engine);
        const double t  = (sr > 0.0) ? (frames / sr) : prevTimeSec;
        double dt = t - prevTimeSec; if (dt < 0.0) dt = 0.0;
        prevTimeSec = t;
        return { (float)dt, (float)t };
    }
};

struct Input
{
	std::string					m_modelPath;
	std::vector<std::string>	m_vmdPaths;
	float						m_scale = 1.0f;
};

void Usage() {
	std::cout << "app [-model <pmd|pmx file path>] [-vmd <vmd file path>]\n";
	std::cout << "e.g. app -model model1.pmx -vmd anim1_1.vmd -vmd anim1_2.vmd  -model model2.pmx\n";
}

bool SampleMain(std::vector<std::string>& args, AudioContext* audioCtx) {
	Input currentInput;
	std::vector<Input> inputModels;
	for (auto argIt = args.begin(); argIt != args.end(); ++argIt) {
		const auto &arg = *argIt;
		if (arg == "-model") {
			if (!currentInput.m_modelPath.empty())
				inputModels.emplace_back(currentInput);

			++argIt;
			if (argIt == args.end()) {
				Usage();
				return false;
			}
			currentInput = Input();
			currentInput.m_modelPath = *argIt;
		} else if (arg == "-vmd") {
			if (currentInput.m_modelPath.empty()) {
				Usage();
				return false;
			}

			++argIt;
			if (argIt == args.end()) {
				Usage();
				return false;
			}
			currentInput.m_vmdPaths.push_back(*argIt);
		} else if (arg == "-scale") {
			++argIt;
			if (argIt == args.end()) {
				Usage();
				return false;
			}
			currentInput.m_scale = std::stof(*argIt);
		}
	}
	if (!currentInput.m_modelPath.empty())
		inputModels.emplace_back(currentInput);

	// Initialize glfw
	if (!glfwInit())
		return false;
	AppContext appContext;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, appContext.m_msaaSamples);

	auto window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
	if (window == nullptr)
		return false;

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
		return false;

	glfwSwapInterval(0);
	glEnable(GL_MULTISAMPLE);

	// Initialize application
	if (!appContext.Setup()) {
		std::cout << "Failed to setup AppContext.\n";
		return false;
	}

	// Load MMD model
	std::vector<Model> models;
	for (const auto &[m_modelPath, m_vmdPaths, m_scale]: inputModels) {
		// Load MMD model
		Model model;
		auto ext = PathUtil::GetExt(m_modelPath);
		if (ext == "pmx") {
			auto pmxModel = std::make_unique<MMDModel>();
			if (!pmxModel->Load(m_modelPath, appContext.m_mmdDir)) {
				std::cout << "Failed to load pmx file.\n";
				return false;
			}
			model.m_mmdModel = std::move(pmxModel);
		} else {
			std::cout << "Unknown file type. [" << ext << "]\n";
			return false;
		}
		model.m_mmdModel->InitializeAnimation();

		// Load VMD animation
		auto vmdAnim = std::make_unique<VMDAnimation>();
		vmdAnim->m_model = model.m_mmdModel;
		for (const auto &vmdPath: m_vmdPaths) {
			VMDFile vmdFile;
			if (!ReadVMDFile(&vmdFile, vmdPath.c_str())) {
				std::cout << "Failed to read VMD file.\n";
				return false;
			}
			if (!vmdAnim->Add(vmdFile)) {
				std::cout << "Failed to add VMDAnimation.\n";
				return false;
			}
			if (!vmdFile.m_cameras.empty()) {
				auto vmdCamAnim = std::make_unique<VMDCameraAnimation>();
				if (!vmdCamAnim->Create(vmdFile))
					std::cout << "Failed to create VMDCameraAnimation.\n";
				appContext.m_vmdCameraAnim = std::move(vmdCamAnim);
			}
		}
		vmdAnim->SyncPhysics(0.0f);

		model.m_vmdAnim = std::move(vmdAnim);

		model.m_scale = m_scale;

		model.Setup(appContext);

		models.emplace_back(std::move(model));
	}

	auto fpsTime = std::chrono::steady_clock::now();
	int fpsFrame = 0;
	auto saveTime = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		auto time = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(time - saveTime).count();
		if (elapsed > 1.0 / 30.0)
			elapsed = 1.0 / 30.0;
		saveTime = time;
		// 원래 코드
		// appContext.m_elapsed = static_cast<float>(elapsed);
		// appContext.m_animTime += static_cast<float>(elapsed);

		auto dt = static_cast<float>(elapsed);
		float t  = appContext.m_animTime + dt;
		// 음악이 있고, 싱크 ON이면 오디오 시간으로 덮어씀
		if (audioCtx && audioCtx->hasMusic && audioCtx->syncToMusic) {
			auto [adt, at] = audioCtx->PullTimes();
			// at: 음악 재생 누적 시간(초), adt: 그 사이의 델타
			// 오디오 스레드와 드리프트 없도록 음수 방지
			if (adt < 0.f) adt = 0.f;
			dt = adt;
			t  = at;
		}
		appContext.m_elapsed  = dt;
		appContext.m_animTime = t;

		glClearColor(0.839f, 0.902f, 0.961f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		appContext.m_screenWidth = width;
		appContext.m_screenHeight = height;

		glViewport(0, 0, width, height);

		// Setup camera
		if (appContext.m_vmdCameraAnim) {
			appContext.m_vmdCameraAnim->Evaluate(appContext.m_animTime * 30.0f);
			const auto mmdCam = appContext.m_vmdCameraAnim->m_camera;
			MMDLookAtCamera lookAtCam(mmdCam);
			appContext.m_viewMat = glm::lookAt(lookAtCam.m_eye, lookAtCam.m_center, lookAtCam.m_up);
			appContext.m_projMat = glm::perspectiveFovRH(mmdCam.m_fov, static_cast<float>(width),
			                                             static_cast<float>(height), 1.0f, 10000.0f);
		} else {
			appContext.m_viewMat = glm::lookAt(glm::vec3(0, 10, 40), glm::vec3(0, 10, 0), glm::vec3(0, 1, 0));
			appContext.m_projMat = glm::perspectiveFovRH(glm::radians(30.0f), static_cast<float>(width),
			                                             static_cast<float>(height), 1.0f, 10000.0f);
		}

		for (auto &model: models) {
			// Update Animation
			model.UpdateAnimation(appContext);

			// Update Vertices
			model.Update();

			// Draw
			model.Draw(appContext);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();

		//FPS
		{
			fpsFrame++;
			auto currentTime = std::chrono::steady_clock::now();
			double deltaTime = std::chrono::duration<double>(currentTime - fpsTime).count();
			if (deltaTime > 1.0) {
				double fps = static_cast<double>(fpsFrame) / deltaTime;
				std::cout << fps << " fps\n";
				fpsFrame = 0;
				fpsTime = currentTime;
			}
		}
	}

	appContext.Clear();

	glfwTerminate();

	return true;
}

int main() {
	auto args = BuildArgsInteractive();

	const AudioChoice music = ChooseMusicInteractive();

	// GL/앱 초기화 전에 오디오 준비 (문제 없지만, 언제든 순서 바꿔도 됨)
	AudioContext audio;
	audio.Init(music);

	if (!SampleMain(args, &audio)) {
		std::cout << "Failed to run.\n";
		audio.Uninit();
		return 1;
	}
	return 0;
}
