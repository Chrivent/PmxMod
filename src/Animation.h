#pragma once

#include <map>
#include <memory>

#include "Reader.h"

class IkSolver;
struct Morph;
class Node;
class Model;

struct NodeAnimationKey {
	int32_t		m_time;
	glm::vec3	m_translate;
	glm::quat	m_rotate;
	std::pair<glm::vec2, glm::vec2>	m_txBezier;
	std::pair<glm::vec2, glm::vec2>	m_tyBezier;
	std::pair<glm::vec2, glm::vec2>	m_tzBezier;
	std::pair<glm::vec2, glm::vec2>	m_rotBezier;

	/// VMD 모션 키에서 노드 애니메이션 키 값을 채운다.
	void ApplyMotion(const VMDReader::VMDMotion& motion);
};

struct MorphAnimationKey {
	int32_t	m_time;
	float	m_morphWeight;
};

struct IKAnimationKey {
	int32_t	m_time;
	bool	m_ikEnable;
};

class Animation {
public:
	std::shared_ptr<Model> m_model;

	/// VMD 데이터를 모델 애니메이션 트랙에 추가한다.
	bool Add(const VMDReader& vmd);
	/// 애니메이션 트랙과 연결 상태를 해제한다.
	void Destroy();
	/// 지정한 시간의 애니메이션 값을 모델에 평가해 적용한다.
	void Evaluate(float t, float animWeight = 1.0f) const;
	/// 물리 상태를 지정한 애니메이션 시간에 맞춰 동기화한다.
	void SyncPhysics(float t) const;

private:
	std::map<std::shared_ptr<Node>, std::vector<NodeAnimationKey>> m_nodes;
	std::map<std::shared_ptr<IkSolver>, std::vector<IKAnimationKey>> m_iks;
	std::map<std::shared_ptr<Morph>, std::vector<MorphAnimationKey>> m_morphs;
};

struct Camera {
	glm::vec3	m_interest = glm::vec3(0, 10, 0);
	glm::vec3	m_rotate = glm::vec3(0, 0, 0);
	float		m_distance = 50;
	float		m_fov = glm::radians(30.0f);

	/// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
	glm::mat4 CalcViewMatrix() const;
};

struct CameraAnimationKey {
	int32_t		m_time;
	glm::vec3	m_interest;
	glm::vec3	m_rotate;
	float		m_distance;
	float		m_fov;
	std::pair<glm::vec2, glm::vec2>	m_ixBezier;
	std::pair<glm::vec2, glm::vec2>	m_iyBezier;
	std::pair<glm::vec2, glm::vec2>	m_izBezier;
	std::pair<glm::vec2, glm::vec2>	m_rotateBezier;
	std::pair<glm::vec2, glm::vec2>	m_distanceBezier;
	std::pair<glm::vec2, glm::vec2>	m_fovBezier;
};

class CameraAnimation {
public:
	Camera m_camera;

	/// VMD 카메라 키를 읽어 카메라 애니메이션을 생성한다.
	bool Create(const VMDReader& vmd);
	/// 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
	void Evaluate(float t);

private:
	std::vector<CameraAnimationKey>	m_keys;
};
