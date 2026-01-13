#pragma once

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

struct VMDBezier {
	float FindBezierX(float time) const;

	glm::vec2	m_cp1;
	glm::vec2	m_cp2;
};

struct VMDNodeAnimationKey {
	void Set(const VMDReader::VMDMotion& motion);

	int32_t		m_time;
	glm::vec3	m_translate;
	glm::quat	m_rotate;

	VMDBezier	m_txBezier;
	VMDBezier	m_tyBezier;
	VMDBezier	m_tzBezier;
	VMDBezier	m_rotBezier;
};

struct VMDMorphAnimationKey {
	int32_t	m_time;
	float	m_weight;
};

struct VMDIKAnimationKey {
	int32_t	m_time;
	bool	m_enable;
};

class VMDNodeController {
public:
	VMDNodeController();

	void Evaluate(float t, float animWeight = 1.0f);

	MMDNode*				m_node;
	std::vector<VMDNodeAnimationKey>	m_keys;
	size_t					m_startKeyIndex;
};

class VMDMorphController {
public:
	VMDMorphController();

	void Evaluate(float t, float animWeight = 1.0f);

	MMDMorph*				m_morph;
	std::vector<VMDMorphAnimationKey>	m_keys;
	size_t					m_startKeyIndex;
};

class VMDIKController {
public:
	VMDIKController();

	void Evaluate(float t, float animWeight = 1.0f);

	MMDIkSolver*			m_ikSolver;
	std::vector<VMDIKAnimationKey>	m_keys;
	size_t					m_startKeyIndex;
};

class VMDAnimation {
public:
	VMDAnimation();

	bool Add(const VMDReader& vmd);
	void Destroy();
	void Evaluate(float t, float animWeight = 1.0f) const;
	void SyncPhysics(float t, int frameCount = 30) const;

private:
	int32_t CalculateMaxKeyTime() const;

public:
	std::shared_ptr<MMDModel>								m_model;
	std::vector<std::unique_ptr<VMDNodeController>>			m_nodeControllers;
	std::vector<std::unique_ptr<VMDIKController>>			m_ikControllers;
	std::vector<std::unique_ptr<VMDMorphController>>		m_morphControllers;
	uint32_t												m_maxKeyTime;
};

struct MMDCamera {
	MMDCamera();

	glm::vec3	m_interest{};
	glm::vec3	m_rotate{};
	float		m_distance;
	float		m_fov;

	glm::mat4 GetViewMatrix() const;
};

struct VMDCameraAnimationKey {
	int32_t		m_time;
	glm::vec3	m_interest;
	glm::vec3	m_rotate;
	float		m_distance;
	float		m_fov;

	VMDBezier	m_ixBezier;
	VMDBezier	m_iyBezier;
	VMDBezier	m_izBezier;
	VMDBezier	m_rotateBezier;
	VMDBezier	m_distanceBezier;
	VMDBezier	m_fovBezier;
};

class VMDCameraController {
public:
	VMDCameraController();

	void Evaluate(float t);
	void AddKey(const VMDCameraAnimationKey& key);
	void SortKeys();

	std::vector<VMDCameraAnimationKey>	m_keys;
	MMDCamera							m_camera;
	size_t								m_startKeyIndex;
};

class VMDCameraAnimation {
public:
	VMDCameraAnimation();

	bool Create(const VMDReader& vmd);
	void Destroy();
	void Evaluate(float t);

	std::unique_ptr<VMDCameraController>	m_cameraController;
	MMDCamera	m_camera;
};
