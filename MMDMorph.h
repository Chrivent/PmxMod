#pragma once

#include <string>
#include <vector>
#include <memory>

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

	class MMDMorphManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindMorphIndex(const std::string& name);
		MMDMorph* GetMorph(size_t idx);
		MMDMorph* GetMorph(const std::string& name);
		MMDMorph* AddMorph();

		std::vector<std::unique_ptr<MMDMorph>>	m_morphs;
	};
}
