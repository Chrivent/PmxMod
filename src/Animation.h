#pragma once

#include <map>

#include "Reader.h"

struct Morph;
class IkSolver;
class Node;
class Model;

class Bezier {
public:
	// VMD 보간 바이트 네 개를 정규화된 2D Bezier 제어점으로 변환한다.
	static void AssignBezier(std::pair<glm::vec2, glm::vec2>& bezier, int x0, int x1, int y0, int y1);
	// 3차 Bezier 곡선의 단일 축 값을 계산한다.
	static float EvaluateBezier(float t, float p1, float p2);
	// 주어진 x 시간에 대응하는 Bezier 매개변수 t를 이분 탐색으로 찾는다.
	static float FindBezierX(float time, float x1, float x2);
};

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

class Animation {
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
	std::pair<glm::vec2, glm::vec2>	ixBezier;
	std::pair<glm::vec2, glm::vec2>	iyBezier;
	std::pair<glm::vec2, glm::vec2>	izBezier;
	std::pair<glm::vec2, glm::vec2>	rotateBezier;
	std::pair<glm::vec2, glm::vec2>	distanceBezier;
	std::pair<glm::vec2, glm::vec2>	fovBezier;
};

class CameraAnimation {
public:
	Camera camera;

	// VMD 카메라 키를 읽어 카메라 애니메이션을 생성한다.
	bool Create(const VmdReader& vmd);
	// 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
	void Evaluate(float t);

private:
	std::vector<CameraAnimationKey>	keys;
};
