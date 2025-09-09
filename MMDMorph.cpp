#include "MMDMorph.h"

namespace saba
{
	MMDMorph::MMDMorph()
		: m_weight(0)
		, m_saveAnimWeight(0)
		, m_morphType()
		, m_dataIndex(0) {
	}

	void MMDMorph::SaveBaseAnimation()
	{
		m_saveAnimWeight = m_weight;
	}

	void MMDMorph::LoadBaseAnimation()
	{
		m_weight = m_saveAnimWeight;
	}

	void MMDMorph::ClearBaseAnimation()
	{
		m_saveAnimWeight = 0;
	}

	size_t MMDMorphManager::FindMorphIndex(const std::string& name)
	{
		const auto findIt = std::ranges::find_if(m_morphs, [&name](const std::unique_ptr<MMDMorph>& morph)
			{ return morph->m_name == name; }
		);
		if (findIt == m_morphs.end())
		{
			return NPos;
		}
		return findIt - m_morphs.begin();
	}

	MMDMorph* MMDMorphManager::GetMorph(const size_t idx) const
	{
		return m_morphs[idx].get();
	}

	MMDMorph* MMDMorphManager::GetMorph(const std::string& name)
	{
		const auto findIdx = FindMorphIndex(name);
		if (findIdx == NPos)
		{
			return nullptr;
		}
		return GetMorph(findIdx);
	}

	MMDMorph* MMDMorphManager::AddMorph()
	{
		m_morphs.emplace_back(std::make_unique<MMDMorph>());
		return m_morphs[m_morphs.size() - 1].get();
	}
}
