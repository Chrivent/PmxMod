#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

#include "base/Path.h"

#include "AppContext.h"
#include "Model.h"

#define MINIAUDIO_IMPLEMENTATION
#include "external/miniaudio.h"

namespace fs = std::filesystem;

// ===== 하드코딩 경로 =====
static const fs::path MODEL_DIR  = "C:/Users/Ha Yechan/Desktop/PMXViewer/models";
static const fs::path MOTION_DIR = "C:/Users/Ha Yechan/Desktop/PMXViewer/motions";
static const fs::path CAMERA_DIR = "C:/Users/Ha Yechan/Desktop/PMXViewer/cameras";
static const fs::path MUSIC_DIR  = "C:/Users/Ha Yechan/Desktop/PMXViewer/musics";

// 소문자화 (ASCII 확장자 비교용)
static std::string tolower_copy(std::string s){
    std::ranges::transform(s, s.begin(), [](const unsigned char c)
    	{ return static_cast<char>(std::tolower(c)); });
    return s;
}

// --- 안전 출력(helper): u8string을 그대로 write ---
static void PrintU8(const std::u8string& s) {
    std::cout.write(reinterpret_cast<const char*>(s.c_str()),
                    static_cast<std::streamsize>(s.size()));
}
static void PrintPathSafe(const fs::path& p) { PrintU8(p.u8string()); }
static void PrintFilenameSafe(const fs::path& p) { PrintU8(p.filename().u8string()); }

// --- 경로 → UTF-8 std::string (args에 넣을 때 필수) ---
static std::string PathToUtf8(const fs::path& p){
    const auto u8 = p.lexically_normal().u8string();                    // basic_string<char8_t>
    return { reinterpret_cast<const char*>(u8.c_str()), u8.size() };
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
    std::ranges::sort(out, [](const fs::path& a, const fs::path& b)
    	{ return a.filename().native() < b.filename().native(); });
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
    std::ranges::sort(out, [](const fs::path& a, const fs::path& b)
    	{ return a.filename().native() < b.filename().native(); });
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
    std::vector<int> indies; int v;
    while (iss >> v) if (v>=0 && static_cast<size_t>(v)<maxN) indies.push_back(v);
    std::ranges::sort(indies);
    indies.erase(std::ranges::unique(indies).begin(), indies.end());
    return indies;
}

static int ReadOneIndex(const std::string& prompt, size_t maxN){
    std::cout << prompt;
    std::string s; std::getline(std::cin, s);
    if (s.empty()) return -1;
    try { const int v = std::stoi(s); if (v>=0 && static_cast<size_t>(v)<maxN) return v; } catch(...) {}
    return -1;
}

// === 핵심: 간단/짧은 인터랙티브 args 빌더 ===
static std::vector<std::string> BuildArgsInteractive(){
    std::vector<std::string> args;

    const auto modelFolders = ListSubdirs(MODEL_DIR);
    const auto motions      = ListFilesWithExt(MOTION_DIR, ".vmd");
    const auto cameras      = ListFilesWithExt(CAMERA_DIR, ".vmd");

    if (modelFolders.empty()){
        std::cout << "[오류] 모델 루트 폴더가 비었어요: ";
        PrintPathSafe(MODEL_DIR); std::cout << "\n";
        return args;
    }

    std::cout << "\n[모델 폴더]\n";
    PrintIndexed(modelFolders, "");
    const auto modelSel = ReadMultiIndex("추가할 모델 폴더 번호들(공백 구분, 비면 취소): ", modelFolders.size());
    if (modelSel.empty()){
        std::cout << "[오류] 모델을 선택하지 않았습니다.\n";
        return args;
    }

    if (!motions.empty()){
        PrintIndexed(motions, "\n[모션 VMD]");
    }
    const auto motionSel = motions.empty() ? std::vector<int>{}
                                     : ReadMultiIndex("이 모델들에 붙일 모션 번호들(공백, 비면 없음): ", motions.size());

    // 각 모델 폴더에서 PMX 하나 자동 선택
    for (const int fIdx : modelSel){
        const fs::path& folder = modelFolders[static_cast<size_t>(fIdx)];
        auto pmxs = ListFilesWithExt(folder, ".pmx");
        if (pmxs.empty()){
            std::cout << "  [건너뜀] ";
            PrintFilenameSafe(folder);
            std::cout << " 폴더에 .pmx 없음.\n";
            continue;
        }
        // 폴더에 하나만 있다고 했으니 첫 번째 사용
        args.emplace_back("-model");
        args.push_back(PathToUtf8(pmxs[0]));  // ← 여기!

        for (const int mi : motionSel){
            args.emplace_back("-vmd");
            args.push_back(PathToUtf8(motions[static_cast<size_t>(mi)])); // ← 여기!
        }
    }

    // 카메라(선택, 0~1)
    if (!cameras.empty()){
        PrintIndexed(cameras, "\n[카메라 VMD]");
        const int cIdx = ReadOneIndex("카메라 번호(엔터=없음): ", cameras.size());
        if (cIdx >= 0){
            args.emplace_back("-vmd"); // 마지막에 추가 → 전역 카메라로 적용
            args.push_back(PathToUtf8(cameras[static_cast<size_t>(cIdx)])); // ← 여기!
        }
    }

	args.emplace_back("-scale");
	args.emplace_back("1.0f");
	args.emplace_back("-model");
	args.emplace_back(R"(C:\Users\Ha Yechan\Desktop\PMXViewer\models\torisutsuki\torisutsuki.pmx)");

    std::cout << "\n[최종 인자]\n";
    for (auto& s : args) std::cout << s << ' ';
    std::cout << "\n";
    return args;
}

static bool IsAudioExt(const std::string& extLower) {
	return extLower == ".wav" || extLower == ".mp3" || extLower == ".ogg" || extLower == ".flac";
}

static std::vector<fs::path> ListAudioFiles(const fs::path& dir){
	std::vector<fs::path> out;
	std::error_code ec;
	if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return out;

	for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
		 it != end; it.increment(ec)){
		if (ec) break;
		if (!it->is_regular_file(ec)) continue;
		std::string ext = tolower_copy(it->path().extension().string());
		if (!IsAudioExt(ext)) continue;
		if (it->path().filename().native().empty()) continue;
		out.push_back(it->path());
		 }
	std::ranges::sort(out, [](const fs::path& a, const fs::path& b)
		{ return a.filename().native() < b.filename().native(); });
	return out;
}

struct AudioChoice {
	fs::path path;     // 비었으면 선택 안 함
	bool     sync = false;
};

static AudioChoice ChooseMusicInteractive(){
	AudioChoice choice;
	auto musics = ListAudioFiles(MUSIC_DIR);
	if (musics.empty()){
		std::cout << "\n[알림] 음악 폴더가 비었어요: "; PrintPathSafe(MUSIC_DIR); std::cout << "\n";
		return choice; // path empty -> 선택 안 함
	}

	PrintIndexed(musics, "\n[음악 파일]");
	const int idx = ReadOneIndex("재생할 음악 번호(엔터=없음): ", musics.size());
	if (idx >= 0){
		choice.path = musics[static_cast<size_t>(idx)];
		choice.sync = true;
	}
	return choice;
}

struct AudioContext {
	ma_engine engine{};
	ma_sound  sound{};
	bool      hasMusic = false;
	bool      syncToMusic = false;
	double    prevTimeSec = 0.0;

	void Init(const AudioChoice& choice){
		if (choice.path.empty()) return;
		if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
			std::cout << "[오디오] ma_engine_init 실패.\n";
			return;
		}
		const std::string u8 = PathToUtf8(choice.path);
		if (ma_sound_init_from_file(&engine, u8.c_str(), 0, nullptr, nullptr, &sound) != MA_SUCCESS){
			std::cout << "[오디오] 파일 열기 실패: "; PrintPathSafe(choice.path); std::cout << "\n";
			ma_engine_uninit(&engine);
			return;
		}
		// 필요하면 루프
		// ma_sound_set_looping(&sound, MA_TRUE);

		ma_sound_start(&sound);
		hasMusic = true;
		syncToMusic = choice.sync;
		prevTimeSec = 0.0;
		std::cout << "[오디오] 재생 시작: "; PrintFilenameSafe(choice.path); std::cout << (syncToMusic ? " (싱크 ON)\n" : " (싱크 OFF)\n");
	}

	void UnInit(){
		if (hasMusic){
			ma_sound_uninit(&sound);
			ma_engine_uninit(&engine);
			hasMusic = false;
		}
	}

	// 음악 시간 기반으로 (delta, total) 제공
	std::pair<float,float> PullTimes(){
		if (!hasMusic) return {0.f, 0.f};

		ma_uint64 cursorFrames = 0;
		if (ma_sound_get_cursor_in_pcm_frames(&sound, &cursorFrames) != MA_SUCCESS)
			return {0.f, static_cast<float>(prevTimeSec)};

		const auto sr = static_cast<double>(ma_engine_get_sample_rate(&engine)); // Hz
		const double t  = sr > 0.0 ? static_cast<double>(cursorFrames) / sr : prevTimeSec; // seconds
		double dt = t - prevTimeSec;
		if (dt < 0.0) dt = 0.0;

		prevTimeSec = t;
		return { static_cast<float>(dt), static_cast<float>(t) };
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
		auto ext = saba::PathUtil::GetExt(m_modelPath);
		if (ext == "pmx") {
			auto pmxModel = std::make_unique<saba::MMDModel>();
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
		auto vmdAnim = std::make_unique<saba::VMDAnimation>();
		vmdAnim->m_model = model.m_mmdModel;
		for (const auto &vmdPath: m_vmdPaths) {
			saba::VMDFile vmdFile;
			if (!saba::ReadVMDFile(&vmdFile, vmdPath.c_str())) {
				std::cout << "Failed to read VMD file.\n";
				return false;
			}
			if (!vmdAnim->Add(vmdFile)) {
				std::cout << "Failed to add VMDAnimation.\n";
				return false;
			}
			if (!vmdFile.m_cameras.empty()) {
				auto vmdCamAnim = std::make_unique<saba::VMDCameraAnimation>();
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
			saba::MMDLookAtCamera lookAtCam(mmdCam);
			appContext.m_viewMat = glm::lookAt(lookAtCam.m_eye, lookAtCam.m_center, lookAtCam.m_up);
			appContext.m_projMat = glm::perspectiveFovRH(mmdCam.m_fov, static_cast<float>(width),
			                                             static_cast<float>(height), 1.0f, 10000.0f);
		} else {
			appContext.m_viewMat = glm::lookAt(glm::vec3(0, 10, 50), glm::vec3(0, 10, 0), glm::vec3(0, 1, 0));
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
		audio.UnInit();
		return 1;
	}
	return 0;
}
