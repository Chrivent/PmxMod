#pragma once

#include <map>

#include "Animation.h"
#include "../Reader.h"

struct Morph;
class IkSolver;
class Node;
class Model;

struct NodeAnimationKey {
	int32_t		time;
	glm::vec3	translate;
	glm::quat	rotate;
	std::pair<glm::vec2, glm::vec2>	txBezier;
	std::pair<glm::vec2, glm::vec2>	tyBezier;
	std::pair<glm::vec2, glm::vec2>	tzBezier;
	std::pair<glm::vec2, glm::vec2>	rotBezier;

	// VMD 모션 키에서 노드 애니메이션 키 값을 채운다.
	void ApplyMotion(const VmdReader::VmdMotion& motion);
};

struct MorphAnimationKey {
	int32_t	time;
	float	morphWeight;
};

struct IkAnimationKey {
	int32_t	time;
	bool	ikEnable;
};

class ModelAnimation : public Animation {
public:
	std::shared_ptr<Model> model;

	// VMD 데이터를 모델 애니메이션 트랙에 추가한다.
	bool Add(const VmdReader& vmd);
	// 애니메이션 트랙과 연결 상태를 해제한다.
	void Destroy();
	// 지정한 시간의 애니메이션 값을 모델에 평가해 적용한다.
	void Evaluate(float t, float animWeight = 1.0f) const;
	// 물리 상태를 지정한 애니메이션 시간에 맞춰 동기화한다.
	void SyncPhysics(float t) const;

private:
	std::map<std::shared_ptr<Node>, std::vector<NodeAnimationKey>> nodes;
	std::map<std::shared_ptr<IkSolver>, std::vector<IkAnimationKey>> iks;
	std::map<std::shared_ptr<Morph>, std::vector<MorphAnimationKey>> morphs;
};
