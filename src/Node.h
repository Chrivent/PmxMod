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

	/// 이 노드에 자식 노드를 연결하고 형제 링크를 갱신한다.
	void AddChild(const std::shared_ptr<Node>& child);
	/// 로컬/글로벌 변환 계산 전에 프레임 상태를 초기화한다.
	void BeginUpdateTransform();
	/// 기본, 애니메이션, IK, 부가 변환을 합쳐 로컬 행렬을 갱신한다.
	void UpdateLocalTransform();
	/// 부모 변환을 반영해 글로벌 행렬을 갱신한다.
	void UpdateGlobalTransform();
	/// 자식 노드들의 변환을 재귀적으로 갱신한다.
	void UpdateChildTransform() const;
	/// 부가 부모 본의 회전/이동 영향을 현재 노드에 적용한다.
	void UpdateAppendTransform();
};
