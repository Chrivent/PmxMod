#pragma once

#include <string>
#include <vector>
#include <memory>

enum class PMXMorphType : uint8_t;

struct MMDMorph
{
	MMDMorph();

	std::string	m_name;
	float		m_weight;
	float		m_saveAnimWeight;
	PMXMorphType	m_morphType;
	size_t		m_dataIndex;
};

class MMDMorphManager
{
public:
	std::vector<std::unique_ptr<MMDMorph>>	m_morphs;
};
