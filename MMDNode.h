#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace saba
{
	class MMDIkSolver;

	class MMDNode
	{
	public:
		MMDNode();

		void AddChild(MMDNode* child);
		// アニメーションの前後て呼ぶ
		void BeginUpdateTransform();

		void UpdateLocalTransform();
		void UpdateGlobalTransform();
		void UpdateChildTransform();

		glm::vec3 AnimateTranslate() const;
		glm::quat AnimateRotate() const;

		void CalculateInverseInitTransform();

		// ノードの初期化時に呼び出す
		void SaveInitialTRS();
		void LoadInitialTRS();
		void SaveBaseAnimation();
		void LoadBaseAnimation();
		void ClearBaseAnimation();

		void UpdateAppendTransform();

	public:
		uint32_t		m_index;
		std::string		m_name;
		bool			m_enableIK;

		MMDNode*		m_parent;
		MMDNode*		m_child;
		MMDNode*		m_next;
		MMDNode*		m_prev;

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

		MMDNode*	m_appendNode;
		bool		m_isAppendRotate;
		bool		m_isAppendTranslate;
		bool		m_isAppendLocal;
		float		m_appendWeight;

		glm::vec3	m_appendTranslate;
		glm::quat	m_appendRotate;

		MMDIkSolver* m_ikSolver;
	};

	class MMDNodeManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindNodeIndex(const std::string& name);
		MMDNode* GetNode(size_t idx);
		MMDNode* GetNode(const std::string& nodeName);
		MMDNode* AddNode();

		std::vector<std::unique_ptr<MMDNode>>	m_nodes;
	};
}

