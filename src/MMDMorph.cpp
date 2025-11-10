#include "MMDMorph.h"

MMDMorph::MMDMorph()
	: m_weight(0)
	, m_saveAnimWeight(0)
	, m_morphType()
	, m_dataIndex(0) {
}

MMDMorph* MMDMorphManager::GetMorph(const std::string& name)
{
	const auto findIt = std::ranges::find_if(m_morphs,
		[&name](const std::unique_ptr<MMDMorph> &morph)
		{ return morph->m_name == name; }
	);
	if (findIt == m_morphs.end())
		return nullptr;
	return m_morphs[findIt - m_morphs.begin()].get();
}
