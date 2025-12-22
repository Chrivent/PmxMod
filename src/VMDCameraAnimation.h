#pragma once

#include "VMDAnimation.h"

#include <memory>

struct MMDCamera
{
	MMDCamera();

	glm::vec3	m_interest{};
	glm::vec3	m_rotate{};
	float		m_distance;
	float		m_fov;

	glm::mat4 GetViewMatrix() const;
};

struct VMDCameraAnimationKey
{
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

class VMDCameraController
{
public:
	VMDCameraController();

	void Evaluate(float t);
	void AddKey(const VMDCameraAnimationKey& key);
	void SortKeys();

	std::vector<VMDCameraAnimationKey>	m_keys;
	MMDCamera							m_camera;
	size_t								m_startKeyIndex;
};

class VMDCameraAnimation
{
public:
	VMDCameraAnimation();

	bool Create(const VMDReader& vmd);
	void Destroy();
	void Evaluate(float t);

	std::unique_ptr<VMDCameraController>	m_cameraController;
	MMDCamera	m_camera;
};
