#pragma once

#include <string>
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
		void EndUpdateTransform();

		void UpdateLocalTransform();
		void UpdateGlobalTransform();
		void UpdateChildTransform();

		glm::vec3 AnimateTranslate() const;
		glm::quat AnimateRotate() const;

		void CalculateInverseInitTransform();

		// ノードの初期化時に呼び出す
		void SaveInitialTRS()
		{
			m_initTranslate = m_translate;
			m_initRotate = m_rotate;
			m_initScale = m_scale;
		}
		void LoadInitialTRS()
		{
			m_translate = m_initTranslate;
			m_rotate = m_initRotate;
			m_scale = m_initScale;
		}

		void SaveBaseAnimation()
		{
			m_baseAnimTranslate = m_animTranslate;
			m_baseAnimRotate = m_animRotate;
		}

		void LoadBaseAnimation()
		{
			m_animTranslate = m_baseAnimTranslate;
			m_animRotate = m_baseAnimRotate;
		}

		void ClearBaseAnimation()
		{
			m_baseAnimTranslate = glm::vec3(0);
			m_baseAnimRotate = glm::quat(1, 0, 0, 0);
		}

		void UpdateAppendTransform();

	protected:
		virtual void OnBeginUpdateTransform();
		virtual void OnEndUpdateTransfrom();
		virtual void OnUpdateLocalTransform();

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
}

