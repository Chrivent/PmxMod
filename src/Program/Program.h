#pragma once

#include "../Viewer/Viewer.h"

namespace Chrivent {
    class Program {
        std::unique_ptr<Viewer> viewer;
        
    public:
        // 씬 설정을 구성하고 선택한 렌더러로 뷰어를 실행한다.
        bool Run();
    };
}
