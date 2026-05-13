#include <iostream>
#include <memory>

#include "viewer/Dx11Viewer.h"
#include "viewer/GlfwViewer.h"

// 씬 설정을 구성하고 선택한 렌더러로 뷰어를 실행한다.
int main() {
	SceneConfig cfg;
	int engineType = 0;
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
