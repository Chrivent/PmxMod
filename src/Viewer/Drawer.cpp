#include "Drawer.h"

namespace Chrivent {
	Drawer::~Drawer() = default;

	void Drawer::Draw() const {
		DrawModel();
		DrawEdge();
		DrawGroundShadow();
	}
}
