#pragma once

#include <string>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

struct IkSolver;

struct Node
{
	Node();

	void AddChild(Node* child);
	void BeginUpdateTransform();
	void UpdateLocalTransform();
	void UpdateGlobalTransform();
	void UpdateChildTransform() const;
	void UpdateAppendTransform();

	uint32_t		m_index;
	std::string		m_name;
	bool			m_enableIK;
	Node*		m_parent;
	Node*		m_child;
	Node*		m_next;
	Node*		m_prev;
	glm::vec3	m_translate;
	glm::quat	m_rotate;
	glm::vec3	m_scale;
	glm::vec3	m_animTranslate;
	glm::quat	m_animRotate;
	glm::vec3	m_baseAnimTranslate;
	glm::quat	m_baseAnimRotate;
	glm::quat	m_ikRotate;
	glm::mat4		m_local;
	glm::mat4		m_global;
	glm::mat4		m_inverseInit;
	glm::vec3	m_initTranslate;
	glm::quat	m_initRotate;
	glm::vec3	m_initScale;
	int32_t		m_deformDepth;
	bool		m_isDeformAfterPhysics;
	Node*	m_appendNode;
	bool		m_isAppendRotate;
	bool		m_isAppendTranslate;
	bool		m_isAppendLocal;
	float		m_appendWeight;
	glm::vec3	m_appendTranslate;
	glm::quat	m_appendRotate;
	IkSolver* m_ikSolver;
};
