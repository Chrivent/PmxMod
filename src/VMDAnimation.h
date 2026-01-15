#pragma once

#include <map>
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include "MMDReader.h"

struct MMDIkSolver;
struct MMDMorph;
struct MMDNode;
class MMDModel;

struct NodeAnimationKey {
	void Set(const VMDReader::VMDMotion& motion);

	int32_t		m_time;
	glm::vec3	m_translate;
	glm::quat	m_rotate;

	std::pair<glm::vec2, glm::vec2>	m_txBezier;
	std::pair<glm::vec2, glm::vec2>	m_tyBezier;
	std::pair<glm::vec2, glm::vec2>	m_tzBezier;
	std::pair<glm::vec2, glm::vec2>	m_rotBezier;
};

struct MorphAnimationKey {
	int32_t	m_time;
	float	m_morphWeight;
};

struct IKAnimationKey {
	int32_t	m_time;
	bool	m_ikEnable;
};

class VMDAnimation {
public:
	bool Add(const VMDReader& vmd);
	void Destroy();
	void Evaluate(float t, float animWeight = 1.0f) const;

	std::shared_ptr<MMDModel>								m_model;
	std::map<MMDNode*, std::vector<NodeAnimationKey>>	m_nodes;
	std::map<MMDIkSolver*, std::vector<IKAnimationKey>>	m_iks;
	std::map<MMDMorph*, std::vector<MorphAnimationKey>>	m_morphs;
};

struct Camera {
	Camera();

	glm::vec3	m_interest{};
	glm::vec3	m_rotate{};
	float		m_distance;
	float		m_fov;

	glm::mat4 GetViewMatrix() const;
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

class VMDCameraAnimation {
public:
	bool Create(const VMDReader& vmd);
	void Evaluate(float t);

	std::vector<CameraAnimationKey>	m_keys;
	Camera	m_camera;
};
