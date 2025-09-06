#pragma once

#include <string>

namespace saba
{
	enum class MorphType
	{
		None,
		Position,
		UV,
		Material,
		Bone,
		Group,
	};

	class MMDMorph
	{
	public:
		MMDMorph();

		void SetWeight(float weight) { m_weight = weight; }
		float GetWeight() const { return m_weight; }

		void SaveBaseAnimation() { m_saveAnimWeight = m_weight; }
		void LoadBaseAnimation() { m_weight = m_saveAnimWeight; }
		void ClearBaseAnimation() { m_saveAnimWeight = 0; }
		float GetBaseAnimationWeight() const { return m_saveAnimWeight; }

	public:
		std::string	m_name;
		float		m_weight;
		float		m_saveAnimWeight;

		MorphType	m_morphType;
		size_t		m_dataIndex;
	};
}
