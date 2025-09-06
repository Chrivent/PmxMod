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

		void SaveBaseAnimation();
		void LoadBaseAnimation();
		void ClearBaseAnimation();

	public:
		std::string	m_name;
		float		m_weight;
		float		m_saveAnimWeight;

		MorphType	m_morphType;
		size_t		m_dataIndex;
	};
}
