#include "Instance.h"

#include "Viewer.h"
#include "../Animation/Animation.h"
#include "../Model/Model.h"

namespace Chrivent {
    Instance::Instance() = default;
    Instance::~Instance() = default;

    void Instance::Draw() const {
        DrawModel();
        DrawEdge();
        DrawGroundShadow();
    }

    void Instance::UpdateAnimation(const Viewer& viewer) const {
        model->BeginAnimation();
        model->UpdateAllAnimation(anim.get(), viewer.animTime * 30.0f, viewer.elapsed);
    }
}
