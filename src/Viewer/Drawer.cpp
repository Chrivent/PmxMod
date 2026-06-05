#include "Drawer.h"

namespace Chrivent {
	Drawer::~Drawer() = default;

	void Drawer::Draw() {
		DrawModel();
		DrawEdge();
		DrawGroundShadow();
	}
}
