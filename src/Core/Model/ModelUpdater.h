#pragma once

#include <chrono>
#include <span>

namespace Chrivent {
	class Animation;
	class Model;
	class ScenePhysics;

	// 모델 갱신 단계별 CPU 소요 시간을 보관한다.
	struct ModelUpdateTiming {
		double initializeMilliseconds = 0.0;
		double animationEvaluateMilliseconds = 0.0;
		double morphMilliseconds = 0.0;
		double beforePhysicsPoseMilliseconds = 0.0;
		double physicsMilliseconds = 0.0;
		double afterPhysicsPoseMilliseconds = 0.0;
		double transformMilliseconds = 0.0;
	};

	// 탐색 시 같은 시점에 평가할 모델과 선택적인 모션을 연결한다.
	struct ModelAnimationTarget {
		Model& model;
		const Animation* animation;
	};

	// 애니메이션, 물리, 포즈와 스키닝 갱신 순서를 조정한다.
	class ModelUpdater {
		// 현재 애니메이션 상태를 기준 애니메이션으로 저장한다.
		static void SaveBaseAnimation(const Model& model);
		// 저장된 기준 애니메이션 상태를 지운다.
		static void ClearBaseAnimation(const Model& model);
		// 프레임 애니메이션 평가를 시작하기 전 상태를 준비한다.
		static void BeginAnimation(Model& model);
		// 단계 경계의 본도 현재 로컬 행렬에서 글로벌 행렬을 다시 계산한다.
		static void UpdateStageTransforms(const Model& model, bool afterPhysics);
		// 글로벌 행렬이 확정된 뒤 해당 단계의 부여 변형과 IK를 순서대로 적용한다.
		static void ApplyStageConstraints(const Model& model, bool afterPhysics);
		static void EvaluateBeforePhysics(Model& model, const Animation* animation, float frame, float weight, ModelUpdateTiming* timing);
		static void EvaluateAfterPhysics(const Model& model, bool reflectPhysics, ModelUpdateTiming* timing);
		// 같은 단계가 나뉘어 실행되더라도 이번 프레임의 측정값에 누적한다.
		template <class Task>
		static void RunStage(double* milliseconds, const Task& task) {
			if (!milliseconds) {
				task();
				return;
			}
			const auto start = std::chrono::steady_clock::now();
			task();
			*milliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}
		// 현재 노드 글로벌 행렬을 스키닝용 최종 본 행렬로 변환한다.
		static void UpdateTransforms(Model& model);

	public:
		// 애니메이션 평가에 필요한 기본 상태를 초기화한다.
		static void InitializeAnimation(Model& model);
		// 대상 포즈에서 월드를 초기화하고 모든 강체를 함께 안정화한다.
		static void ResetPhysicsAtFrame(const ScenePhysics& scene, std::span<const ModelAnimationTarget> targets, float frame);
		// 씬 스텝 전에 모션·모프와 물리 전 본을 평가한다.
		static void PrepareBeforePhysics(Model& model, const Animation* animation, float frame, ModelUpdateTiming* timing = nullptr);
		// 씬 스텝 후 물리·본 결과로 현재 프레임의 스키닝 작업을 준비한다.
		static void PrepareAfterPhysics(Model& model, bool reflectPhysics, bool preservePreviousPositions, ModelUpdateTiming* timing = nullptr);
		// 현재 모델의 정점 갱신 범위를 기준으로 스키닝 작업 수를 계산한다.
		static std::size_t CalculateSkinningTaskCount(const Model& model);
		// 지정된 범위의 CPU 스키닝을 수행한다.
		static void UpdateSkinning(Model& model, std::size_t taskIndex);
	};
}
