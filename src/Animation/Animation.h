#pragma once

#include "Bezier.h"
#include "../Reader/VmdReader.h"

struct Morph;
class IkSolver;
class Node;
class Model;

struct NodeAnimationKey {
	int32_t		time;
	glm::vec3	translate;
	glm::quat	rotate;
	Bezier		txBezier;
	Bezier		tyBezier;
	Bezier		tzBezier;
	Bezier		rotBezier;

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

struct NodeAnimationTrack {
	std::shared_ptr<Node> node;
	std::vector<NodeAnimationKey> keys;
};

struct IkAnimationTrack {
	std::shared_ptr<IkSolver> ikSolver;
	std::vector<IkAnimationKey> keys;
};

struct MorphAnimationTrack {
	std::shared_ptr<Morph> morph;
	std::vector<MorphAnimationKey> keys;
};

class Animation {
	std::vector<NodeAnimationTrack> nodeTracks;
	std::vector<IkAnimationTrack> ikTracks;
	std::vector<MorphAnimationTrack> morphTracks;
	
	// 이름과 일치하는 모델 노드를 찾는다.
	std::shared_ptr<Node> FindNodeByName(const std::string& name) const;
	// 이름과 일치하는 IK 솔버를 찾는다.
	std::shared_ptr<IkSolver> FindIkSolverByName(const std::string& name) const;
	// 이름과 일치하는 모델 모프를 찾는다.
	std::shared_ptr<Morph> FindMorphByName(const std::string& name) const;
	// VMD 본 모션을 노드 애니메이션 트랙에 병합한다.
	void AddNodeAnimations(const VmdReader& vmd);
	// VMD IK 키를 IK 애니메이션 트랙에 병합한다.
	void AddIkAnimations(const VmdReader& vmd);
	// VMD 모프 키를 모프 애니메이션 트랙에 병합한다.
	void AddMorphAnimations(const VmdReader& vmd);
	// 지정 시간의 노드 애니메이션을 평가한다.
	void EvaluateNodes(float t, float animWeight) const;
	// 지정 시간의 IK 애니메이션을 평가한다.
	void EvaluateIks(float t, float animWeight) const;
	// 지정 시간의 모프 애니메이션을 평가한다.
	void EvaluateMorphs(float t, float animWeight) const;
	
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
};
