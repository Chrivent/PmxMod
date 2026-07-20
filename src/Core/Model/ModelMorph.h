#pragma once

#include "Core/Model/Model.h"

namespace Chrivent {
	// 현재 모프 가중치를 모델의 형상, 재질과 본에 누적한다.
	class ModelMorph {
		using PendingMorph = std::pair<const Morph*, float>;

		Model& model;
		std::vector<PendingMorph> pendingMorphs;

		// 재질 모프의 곱셈 계수를 가중치만큼 누적한다.
		static void AccumulateMaterialMul(MaterialMorph& out, const MaterialMorph& val, float weight);
		// 재질 모프의 덧셈 계수를 가중치만큼 누적한다.
		static void AccumulateMaterialAdd(MaterialMorph& out, const MaterialMorph& val, float weight);
		// 단일 모프를 지정한 가중치로 평가한다.
		void EvalMorph(const Morph* morph, float morphWeight);
		// 위치 모프 데이터를 버텍스 위치에 적용한다.
		void MorphPosition(const std::vector<PositionMorph>& morphData, float weight) const;
		// UV 모프 데이터를 버텍스 UV에 적용한다.
		void MorphUv(const std::vector<UvMorph>& morphData, float weight) const;
		// 재질 모프 누적을 시작하기 위해 재질 계수를 초기화한다.
		void BeginMorphMaterial() const;
		// 누적된 재질 모프 결과를 최종 재질에 반영한다.
		void EndMorphMaterial() const;
		// 재질 모프 데이터를 현재 재질 계수에 누적한다.
		void MorphMaterial(const std::vector<MaterialMorph>& morphData, float weight) const;
		// 본 모프 데이터를 노드 애니메이션 변환에 적용한다.
		void MorphBone(const std::vector<BoneMorph>& morphData, float weight) const;

	public:
		explicit ModelMorph(Model& model) : model(model) {}

		// 현재 모프 가중치를 반영해 모델의 모프 애니메이션 결과를 갱신한다.
		void Update();
	};
}
