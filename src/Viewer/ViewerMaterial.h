#pragma once

namespace Chrivent {
	struct Material;

	struct ViewerMaterial {
		const Material& mat;

		explicit ViewerMaterial(const Material& sourceMat) : mat(sourceMat) {}
		virtual ~ViewerMaterial() = default;
	};
}
