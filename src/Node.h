#pragma once

#include <memory>
#include <string>
#include <glm/gtc/quaternion.hpp>

class IkSolver;

class Node : public std::enable_shared_from_this<Node> {
public:
	uint32_t				index = 0;
	std::string				name;
	bool					enableIK = false;
	std::weak_ptr<Node>		parent;
	std::weak_ptr<Node>		child;
	std::weak_ptr<Node>		next;
	std::weak_ptr<Node>		prev;
	glm::vec3				translate = glm::vec3(0);
	glm::quat				rotate = glm::quat(1, 0, 0, 0);
	glm::vec3				scale = glm::vec3(1);
	glm::vec3				animTranslate = glm::vec3(0);
	glm::quat				animRotate = glm::quat(1, 0, 0, 0);
	glm::vec3				baseAnimTranslate = glm::vec3(0);
	glm::quat				baseAnimRotate = glm::quat(1, 0, 0, 0);
	glm::quat				ikRotate = glm::quat(1, 0, 0, 0);
	glm::mat4				local = glm::mat4(1);
	glm::mat4				global = glm::mat4(1);
	glm::mat4				inverseInit = glm::mat4(1);
	glm::vec3				initTranslate = glm::vec3(0);
	glm::quat				initRotate = glm::quat(1, 0, 0, 0);
	glm::vec3				initScale = glm::vec3(1);
	int32_t					deformDepth = -1;
	bool					isDeformAfterPhysics = false;
	std::weak_ptr<Node>		appendNode;
	bool					isAppendRotate = false;
	bool					isAppendTranslate = false;
	bool					isAppendLocal = false;
	float					appendWeight = 0;
	glm::vec3				appendTranslate = glm::vec3(0);
	glm::quat				appendRotate = glm::quat(1, 0, 0, 0);
	std::weak_ptr<IkSolver>	ikSolver;

	/// 이 노드에 자식 노드를 연결하고 형제 링크를 갱신한다.
	void AddChild(const std::shared_ptr<Node>& childNode);
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
