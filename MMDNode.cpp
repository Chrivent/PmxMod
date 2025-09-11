#include "MMDNode.h"

#include <glm/gtc/matrix_transform.hpp>

namespace saba
{
	MMDNode::MMDNode()
		: m_index(0)
		, m_enableIK(false)
		, m_parent(nullptr)
		, m_child(nullptr)
		, m_next(nullptr)
		, m_prev(nullptr)
		, m_translate(0)
		, m_rotate(1, 0, 0, 0)
		, m_scale(1)
		, m_animTranslate(0)
		, m_animRotate(1, 0, 0, 0)
		, m_baseAnimTranslate(0)
		, m_baseAnimRotate(1, 0, 0, 0)
		, m_ikRotate(1, 0, 0, 0)
		, m_local(1)
		, m_global(1)
		, m_inverseInit(1)
		, m_initTranslate(0)
		, m_initRotate(1, 0, 0, 0)
		, m_initScale(1)
		, m_deformDepth(-1)
		, m_isDeformAfterPhysics(false)
		, m_appendNode(nullptr)
		, m_isAppendRotate(false)
		, m_isAppendTranslate(false)
		, m_isAppendLocal(false)
		, m_appendWeight(0)
		, m_appendTranslate()
		, m_appendRotate()
		, m_ikSolver(nullptr) {
	}

	void MMDNode::AddChild(MMDNode * child) {
		if (child == nullptr)
			return;

		child->m_parent = this;
		if (m_child == nullptr) {
			m_child = child;
			m_child->m_next = nullptr;
			m_child->m_prev = m_child;
		} else {
			const auto lastNode = m_child->m_prev;
			lastNode->m_next = child;
			child->m_prev = lastNode;

			m_child->m_prev = child;
		}
	}

	void MMDNode::BeginUpdateTransform() {
		LoadInitialTRS();
		m_ikRotate = glm::quat(1, 0, 0, 0);
		m_appendTranslate = glm::vec3(0);
		m_appendRotate = glm::quat(1, 0, 0, 0);
	}

	void MMDNode::UpdateLocalTransform() {
		glm::vec3 t = AnimateTranslate();
		if (m_isAppendTranslate)
			t += m_appendTranslate;

		glm::quat r = AnimateRotate();
		if (m_enableIK)
			r = m_ikRotate * r;
		if (m_isAppendRotate)
			r = r * m_appendRotate;

		const glm::vec3 s = m_scale;

		m_local = glm::translate(glm::mat4(1), t)
				* glm::mat4_cast(r)
				* glm::scale(glm::mat4(1), s);
	}

	void MMDNode::UpdateGlobalTransform() {
		if (m_parent == nullptr)
			m_global = m_local;
		else
			m_global = m_parent->m_global * m_local;
		MMDNode *child = m_child;
		while (child != nullptr) {
			child->UpdateGlobalTransform();
			child = child->m_next;
		}
	}

	void MMDNode::UpdateChildTransform() const {
		MMDNode *child = m_child;
		while (child != nullptr) {
			child->UpdateGlobalTransform();
			child = child->m_next;
		}
	}

	glm::vec3 MMDNode::AnimateTranslate() const {
		return m_animTranslate + m_translate;
	}

	glm::quat MMDNode::AnimateRotate() const {
		return m_animRotate * m_rotate;
	}

	void MMDNode::CalculateInverseInitTransform() {
		m_inverseInit = glm::inverse(m_global);
	}

	// ノードの初期化時に呼び出す
	void MMDNode::SaveInitialTRS() {
		m_initTranslate = m_translate;
		m_initRotate = m_rotate;
		m_initScale = m_scale;
	}

	void MMDNode::LoadInitialTRS() {
		m_translate = m_initTranslate;
		m_rotate = m_initRotate;
		m_scale = m_initScale;
	}

	void MMDNode::SaveBaseAnimation() {
		m_baseAnimTranslate = m_animTranslate;
		m_baseAnimRotate = m_animRotate;
	}

	void MMDNode::LoadBaseAnimation() {
		m_animTranslate = m_baseAnimTranslate;
		m_animRotate = m_baseAnimRotate;
	}

	void MMDNode::ClearBaseAnimation() {
		m_baseAnimTranslate = glm::vec3(0);
		m_baseAnimRotate = glm::quat(1, 0, 0, 0);
	}

	void MMDNode::UpdateAppendTransform() {
		if (m_appendNode == nullptr)
			return;

		if (m_isAppendRotate) {
			glm::quat appendRotate;
			if (m_isAppendLocal)
				appendRotate = m_appendNode->AnimateRotate();
			else if (m_appendNode->m_appendNode != nullptr)
				appendRotate = m_appendNode->m_appendRotate;
			else
				appendRotate = m_appendNode->AnimateRotate();

			if (m_appendNode->m_enableIK)
				appendRotate = m_appendNode->m_ikRotate * appendRotate;

			const glm::quat appendQ = glm::slerp(
				glm::quat(1, 0, 0, 0),
				appendRotate,
				m_appendWeight
			);
			m_appendRotate = appendQ;
		}

		if (m_isAppendTranslate) {
			glm::vec3 appendTranslate;
			if (m_isAppendLocal)
				appendTranslate = m_appendNode->m_translate - m_appendNode->m_initTranslate;
			else if (m_appendNode->m_appendNode != nullptr)
				appendTranslate = m_appendNode->m_appendTranslate;
			else
				appendTranslate = m_appendNode->m_translate - m_appendNode->m_initTranslate;

			m_appendTranslate = appendTranslate * m_appendWeight;
		}

		UpdateLocalTransform();
	}

	size_t MMDNodeManager::FindNodeIndex(const std::string& name) {
		const auto findIt = std::ranges::find_if(m_nodes,
			[&name](const std::unique_ptr<MMDNode> &node) { return node->m_name == name; }
		);
		if (findIt == m_nodes.end())
			return NPos;
		return findIt - m_nodes.begin();
	}

	MMDNode* MMDNodeManager::GetNodeByIndex(const size_t idx) const {
		return m_nodes[idx].get();
	}

	MMDNode* MMDNodeManager::GetNodeByName(const std::string& nodeName) {
		const auto findIdx = FindNodeIndex(nodeName);
		if (findIdx == NPos)
			return nullptr;
		return GetNodeByIndex(findIdx);
	}

	MMDNode* MMDNodeManager::AddNode() {
		auto node = std::make_unique<MMDNode>();
		node->m_index = static_cast<uint32_t>(m_nodes.size());
		m_nodes.emplace_back(std::move(node));
		return m_nodes[m_nodes.size() - 1].get();
	}
}
