#pragma once

#include "Core/Model/ModelTypes.h"

#include <utility>
#include <vector>

namespace Chrivent {
	class Model;
	struct Morph;

	// 현재 모프 가중치를 모델의 형상, 재질과 본에 누적한다.
	class ModelMorph {
		using PendingMorph = std::pair<const Morph*, float>;

		std::vector<PendingMorph> pendingMorphs;

		// 재질 모프의 곱셈 계수를 가중치만큼 누적한다.
		static void AccumulateMaterialMul(MaterialMorph& out, const MaterialMorph& val, float weight);
		// 재질 모프의 덧셈 계수를 가중치만큼 누적한다.
		static void AccumulateMaterialAdd(MaterialMorph& out, const MaterialMorph& val, float weight);
		// 단일 모프를 지정한 가중치로 평가한다.
		void EvaluateMorph(Model& model, const Morph* morph, float morphWeight);
		// 위치 모프 데이터를 버텍스 위치에 적용한다.
		static void MorphPosition(Model& model, const std::vector<PositionMorph>& morphData, float weight);
		// UV 모프 데이터를 버텍스 UV에 적용한다.
		static void MorphUv(Model& model, const std::vector<UvMorph>& morphData, float weight);
		// 재질 모프 누적을 시작하기 위해 재질 계수를 초기화한다.
		static void BeginMorphMaterial(Model& model);
		// 누적된 재질 모프 결과를 최종 재질에 반영한다.
		static void EndMorphMaterial(Model& model);
		// 재질 모프 데이터를 현재 재질 계수에 누적한다.
		static void MorphMaterial(Model& model, const std::vector<MaterialMorph>& morphData, float weight);
		// 본 모프 데이터를 노드 애니메이션 변환에 적용한다.
		static void MorphBone(const Model& model, const std::vector<BoneMorph>& morphData, float weight);

	public:
		// 현재 모프 가중치를 반영해 모델의 모프 애니메이션 결과를 갱신한다.
		void Update(Model& model);
	};
}
