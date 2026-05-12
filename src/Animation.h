#pragma once

#include <map>

#include "Reader.h"

struct Morph;
class IkSolver;
class Node;
class Model;

struct Bezier {
	glm::vec2	p1;
	glm::vec2	p2;

	// VMD 보간 바이트 네 개를 정규화된 2D Bezier 제어점으로 변환한다.
	void Assign(int x0, int x1, int y0, int y1);
	// 주어진 x 시간에 대응하는 Bezier 보간 값을 계산한다.
	float Evaluate(float time) const;

private:
	// 3차 Bezier 곡선의 단일 축 값을 계산한다.
	static float EvaluateBezier(float t, float p1, float p2);
	// 주어진 x 시간에 대응하는 Bezier 매개변수 t를 이분 탐색으로 찾는다.
	static float FindBezierX(float time, float x1, float x2);
};

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

class Animation {
	std::map<std::shared_ptr<Node>, std::vector<NodeAnimationKey>> nodes;
	std::map<std::shared_ptr<IkSolver>, std::vector<IkAnimationKey>> iks;
	std::map<std::shared_ptr<Morph>, std::vector<MorphAnimationKey>> morphs;

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

struct Camera {
	glm::vec3	interest = glm::vec3(0, 10, 0);
	glm::vec3	rotate = glm::vec3(0, 0, 0);
	float		distance = 50;
	float		fov = glm::radians(30.0f);

	// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
	glm::mat4 CalcViewMatrix() const;
};

struct CameraAnimationKey {
	int32_t		time;
	glm::vec3	interest;
	glm::vec3	rotate;
	float		distance;
	float		fov;
	Bezier		ixBezier;
	Bezier		iyBezier;
	Bezier		izBezier;
	Bezier		rotateBezier;
	Bezier		distanceBezier;
	Bezier		fovBezier;
};

class CameraAnimation {
	std::vector<CameraAnimationKey>	keys;
	
public:
	Camera camera;

	// VMD 카메라 키를 읽어 카메라 애니메이션을 생성한다.
	bool Create(const VmdReader& vmd);
	// 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
	void Evaluate(float t);
};
