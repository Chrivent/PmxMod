#include "Program.h"

#include "Dx11Viewer.h"
#include "GlfwViewer.h"

#include <iostream>

namespace Chrivent {
    bool Program::Run() {
        int engineType = 0;
        if (!(std::cin >> engineType)) {
            std::cin.clear();
            engineType = 0;
        }
        if (engineType == 0)
            viewer = std::make_unique<GlfwViewer>();
        else if (engineType == 1)
            viewer = std::make_unique<Dx11Viewer>();
        const SceneConfig cfg;
        if (viewer && !viewer->Run(cfg)) {
            std::cout << "Failed to run.\n";
            return false;
        }
        return true;
    }
}
