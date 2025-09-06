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
}
