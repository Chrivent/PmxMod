#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <shobjidl.h>

#include "viewer/DX11Viewer.h"
#include "viewer/GLFWViewer.h"

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

static SceneConfig BuildTestSceneConfig() {
	SceneConfig cfg;
	Input in1;
	in1.m_modelPath = R"(D:\예찬\MMD\model\Booth\else\Kamile Yume\Kamile Yume.pmx)";
	in1.m_vmdPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Haku.vmd)");
	Input in2;
	in2.m_modelPath = R"(D:\예찬\MMD\model\Booth\else\Poongpoong Kyoko\Poongpoong Kyoko.pmx)";
	in2.m_vmdPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Luka.vmd)");
	Input in3;
	in3.m_modelPath = R"(D:\예찬\MMD\model\Booth\else\Yeonyuwi Milk\Yeonyuwi Milk.pmx)";
	in3.m_vmdPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Miku.vmd)");
	Input in4;
	in4.m_modelPath = R"(D:\예찬\MMD\model\Booth\else\Mimyung Chronos\Mimyung Chronos.pmx)";
	in4.m_vmdPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Rin.vmd)");
	Input in5;
	in5.m_modelPath = R"(D:\예찬\MMD\model\Booth\Chrivent Elf\Chrivent Elf.pmx)";
	in5.m_vmdPaths.emplace_back(R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\配布用Tda\Teto.vmd)");
	cfg.models.emplace_back(std::move(in1));
	cfg.models.emplace_back(std::move(in2));
	cfg.models.emplace_back(std::move(in3));
	cfg.models.emplace_back(std::move(in4));
	cfg.models.emplace_back(std::move(in5));
	cfg.cameraVmd = R"(D:\예찬\MMD\motion\Kimagure Mercy motion配布用\camera.vmd)";
	cfg.musicPath = R"(D:\예찬\MMD\wav\Kimagure Mercy.wav)";
	Input bg;
	bg.m_modelPath = R"(C:\Users\Ha Yechan\Desktop\sdfa\edit pv song\stage.pmx)";
	bg.m_scale = 1.0f;
	cfg.models.emplace_back(std::move(bg));
	return cfg;
}

int main() {
	constexpr COMDLG_FILTERSPEC kModelFilters[]  = { {L"PMX Model", L"*.pmx"} };
	constexpr COMDLG_FILTERSPEC kVMDFilters[]    = { {L"VMD Motion/Camera", L"*.vmd"} };
	constexpr COMDLG_FILTERSPEC kMusicFilters[]  = { {L"Audio", L"*.wav;*.mp3;*.ogg;*.flac"} };
	const bool kTestMode = true;
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
	int engineType;
	std::cin >> engineType;
	std::unique_ptr<Viewer> viewer;
	if (engineType == 0)
		viewer = std::make_unique<GLFWViewer>();
	else if (engineType == 1)
		viewer = std::make_unique<DX11Viewer>();
	if (viewer && !viewer->Run(cfg)) {
		std::cout << "Failed to run.\n";
		return 1;
	}
	return 0;
}
