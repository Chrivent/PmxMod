#pragma once

#include "../../Parser/PmxParser.h"

namespace Chrivent {
	class ViewerTextureMode {
	public:
		// 기본 텍스처의 존재 여부와 알파 정보로 shader texture mode를 계산한다.
		static int Base(bool hasTexture, bool hasAlpha) {
			if (!hasTexture)
				return 0;
			return hasAlpha ? 2 : 1;
		}

		// toon 텍스처의 존재 여부로 shader texture mode를 계산한다.
		static int Toon(const bool hasTexture) {
			return hasTexture ? 1 : 0;
		}

		// sphere 텍스처의 합성 방식으로 shader texture mode를 계산한다.
		static int Sphere(const bool hasTexture, const SphereMode mode) {
			if (!hasTexture)
				return 0;
			if (mode == SphereMode::Mul)
				return 1;
			if (mode == SphereMode::Add)
				return 2;
			return 0;
		}
	};
}
