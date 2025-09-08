#include "MMDMorph.h"

namespace saba
{
	MMDMorph::MMDMorph()
		: m_weight(0)
		, m_saveAnimWeight(0)
	{
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
		auto findIt = std::find_if(
			m_morphs.begin(),
			m_morphs.end(),
			[&name](const std::unique_ptr<MMDMorph>& morph) { return morph->m_name == name; }
		);
		if (findIt == m_morphs.end())
		{
			return NPos;
		}
		else
		{
			return findIt - m_morphs.begin();
		}
	}

	MMDMorph* MMDMorphManager::GetMorph(size_t idx)
	{
		return m_morphs[idx].get();
	}

	MMDMorph* MMDMorphManager::GetMorph(const std::string& name)
	{
		auto findIdx = FindMorphIndex(name);
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
