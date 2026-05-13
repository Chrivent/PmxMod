#include <iostream>
#include <shobjidl.h>

#include "viewer/Dx11Viewer.h"
#include "viewer/GlfwViewer.h"

// Windows 파일 선택 대화상자를 열어 선택된 파일 경로들을 반환한다.
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

// 개발 테스트용 다중 모델 씬 설정을 생성한다.
static SceneConfig BuildTestSceneConfig1() {
	SceneConfig cfg;
	ModelConfig md1;
	md1.modelPath = R"(D:\예찬\MMD\model\Booth\else\Kamile Yume\Kamile Yume.pmx)";
	md1.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Haku.vmd)");
	md1.scale = 1.1f;
	ModelConfig md2;
	md2.modelPath = R"(D:\예찬\MMD\model\Booth\else\Poongpoong Kyoko\Poongpoong Kyoko.pmx)";
	md2.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Luka.vmd)");
	md2.scale = 1.1f;
	ModelConfig md3;
	md3.modelPath = R"(D:\예찬\MMD\model\Booth\else\Yeonyuwi Milk\Yeonyuwi Milk.pmx)";
	md3.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Miku.vmd)");
	md3.scale = 1.1f;
	ModelConfig md4;
	md4.modelPath = R"(D:\예찬\MMD\model\Booth\else\Mimyung Chronos\Mimyung Chronos.pmx)";
	md4.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Rin.vmd)");
	md4.scale = 1.1f;
	ModelConfig md5;
	md5.modelPath = R"(D:\예찬\MMD\model\Booth\Chrivent Elf\Chrivent Elf.pmx)";
	md5.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Teto.vmd)");
	md5.scale = 1.1f;
	cfg.modelConfigs.emplace_back(std::move(md1));
	cfg.modelConfigs.emplace_back(std::move(md2));
	cfg.modelConfigs.emplace_back(std::move(md3));
	cfg.modelConfigs.emplace_back(std::move(md4));
	cfg.modelConfigs.emplace_back(std::move(md5));
	cfg.cameraAnim = R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\camera.vmd)";
	cfg.musicPath = R"(D:\예찬\MMD\wav\Kimagure Mercy.wav)";
	ModelConfig bg;
	bg.modelPath = R"(C:\Users\Ha Yechan\Desktop\sdfa\edit pv song\stage.pmx)";
	bg.scale = 1.0f;
	cfg.modelConfigs.emplace_back(std::move(bg));
	return cfg;
}

// 개발 테스트용 단일 모델 씬 설정을 생성한다.
static SceneConfig BuildTestSceneConfig2() {
	SceneConfig cfg;
	ModelConfig md;
	md.modelPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\models\Kisaki\Kisaki.pmx)";
	md.animPaths.emplace_back(R"(C:\Users\Ha Yechan\Desktop\PMXViewer\motions\(4)GokurakuJodo.vmd)");
	md.scale = 1.1f;
	cfg.modelConfigs.emplace_back(std::move(md));
	cfg.cameraAnim = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\cameras\(4)GokurakuJodo_camera.vmd)";
	cfg.musicPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\musics\04.wav)";
	ModelConfig bg;
	bg.modelPath = R"(C:\Users\Ha Yechan\Desktop\PMXViewer\backgrounds\torisutsuki\torisutsuki.pmx)";
	bg.scale = 1.0f;
	cfg.modelConfigs.emplace_back(std::move(bg));
	return cfg;
}

// 개발 테스트용 3인 모델 씬 설정을 생성한다.
static SceneConfig BuildTestSceneConfig3() {
	SceneConfig cfg;
	ModelConfig md1;
	md1.modelPath = R"(D:\예찬\MMD\model\Blue Archive\Maid Momoi\Maid Momoi 1.0 T_Pose.pmx)";
	md1.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Dance Robot Dance\mmd_DanceRobotDance_P1.vmd)");
	md1.scale = 1.2f;
	ModelConfig md2;
	md2.modelPath = R"(D:\예찬\MMD\model\Blue Archive\Maid Midori\Maid Midori 1.0 T_Pose.pmx)";
	md2.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Dance Robot Dance\mmd_DanceRobotDance_P2.vmd)");
	md2.scale = 1.2f;
	ModelConfig md3;
	md3.modelPath = R"(D:\예찬\MMD\model\Blue Archive\Kokona 1.0\Kokona 1.0_T.pmx)";
	md3.animPaths.emplace_back(R"(D:\예찬\MMD\motion\Dance Robot Dance\mmd_DanceRobotDance_P3.vmd)");
	md3.scale = 1.2f;
	cfg.modelConfigs.emplace_back(std::move(md1));
	cfg.modelConfigs.emplace_back(std::move(md2));
	cfg.modelConfigs.emplace_back(std::move(md3));
	cfg.cameraAnim = R"(D:\예찬\MMD\motion\Dance Robot Dance\DanceRobotDance_Milo_Camera.vmd)";
	cfg.musicPath = R"(D:\예찬\MMD\motion\Dance Robot Dance\0068_01.wav)";
	ModelConfig bg;
	bg.modelPath = R"(C:\Users\Ha Yechan\Desktop\sdfa\edit pv song\stage.pmx)";
	bg.scale = 1.0f;
	cfg.modelConfigs.emplace_back(std::move(bg));
	return cfg;
}

// 씬 설정을 구성하고 선택한 렌더러로 뷰어를 실행한다.
int main() {
	constexpr COMDLG_FILTERSPEC kModelFilters[]  = { {L"PMX Model", L"*.pmx"} };
	constexpr COMDLG_FILTERSPEC kVMDFilters[]    = { {L"VMD Motion/Camera", L"*.vmd"} };
	constexpr COMDLG_FILTERSPEC kMusicFilters[]  = { {L"Audio", L"*.wav;*.mp3;*.ogg;*.flac"} };
	const bool kTestMode = true;
	SceneConfig cfg;
	if (kTestMode)
		cfg = BuildTestSceneConfig1();
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
			ModelConfig in;
			in.modelPath = modelPaths[i].front().wstring();
			for (auto& v : motionPaths[i])
				in.animPaths.emplace_back(v.wstring());
			cfg.modelConfigs.emplace_back(std::move(in));
		}
		if (!bgPath.empty()) {
			ModelConfig bg;
			bg.modelPath = bgPath.front().wstring();
			bg.scale = 1.0f;
			cfg.modelConfigs.emplace_back(std::move(bg));
		}
		if (!cameraPath.empty())
			cfg.cameraAnim = cameraPath.front().wstring();
		if (!musicPath.empty())
			cfg.musicPath = musicPath.front().wstring();
	}
	int engineType = 0;
	if (!(std::cin >> engineType)) {
		std::cerr << "Failed to read engine type.\n";
		return 1;
	}
	std::unique_ptr<Viewer> viewer;
	if (engineType == 0)
		viewer = std::make_unique<GlfwViewer>();
	else if (engineType == 1)
		viewer = std::make_unique<Dx11Viewer>();
	if (viewer && !viewer->Run(cfg)) {
		std::cout << "Failed to run.\n";
		return 1;
	}
	return 0;
}
