#pragma once

#include <memory>
#include <string>
#include <glm/gtc/quaternion.hpp>

struct IkSolver;

struct Node : std::enable_shared_from_this<Node> {
	uint32_t				m_index = 0;
	std::string				m_name;
	bool					m_enableIK = false;
	std::weak_ptr<Node>		m_parent;
	std::weak_ptr<Node>		m_child;
	std::weak_ptr<Node>		m_next;
	std::weak_ptr<Node>		m_prev;
	glm::vec3				m_translate = glm::vec3(0);
	glm::quat				m_rotate = glm::quat(1, 0, 0, 0);
	glm::vec3				m_scale = glm::vec3(1);
	glm::vec3				m_animTranslate = glm::vec3(0);
	glm::quat				m_animRotate = glm::quat(1, 0, 0, 0);
	glm::vec3				m_baseAnimTranslate = glm::vec3(0);
	glm::quat				m_baseAnimRotate = glm::quat(1, 0, 0, 0);
	glm::quat				m_ikRotate = glm::quat(1, 0, 0, 0);
	glm::mat4				m_local = glm::mat4(1);
	glm::mat4				m_global = glm::mat4(1);
	glm::mat4				m_inverseInit = glm::mat4(1);
	glm::vec3				m_initTranslate = glm::vec3(0);
	glm::quat				m_initRotate = glm::quat(1, 0, 0, 0);
	glm::vec3				m_initScale = glm::vec3(1);
	int32_t					m_deformDepth = -1;
	bool					m_isDeformAfterPhysics = false;
	std::weak_ptr<Node>		m_appendNode;
	bool					m_isAppendRotate = false;
	bool					m_isAppendTranslate = false;
	bool					m_isAppendLocal = false;
	float					m_appendWeight = 0;
	glm::vec3				m_appendTranslate = glm::vec3(0);
	glm::quat				m_appendRotate = glm::quat(1, 0, 0, 0);
	std::weak_ptr<IkSolver>	m_ikSolver;

	void AddChild(const std::shared_ptr<Node>& child);
	void BeginUpdateTransform();
	void UpdateLocalTransform();
	void UpdateGlobalTransform();
	void UpdateChildTransform() const;
	void UpdateAppendTransform();
};
