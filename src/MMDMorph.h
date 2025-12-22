#pragma once

#include <string>
#include <memory>

enum class MorphType : uint8_t;

struct MMDMorph
{
	MMDMorph();

	std::string	m_name;
	float		m_weight;
	float		m_saveAnimWeight;
	MorphType	m_morphType;
	size_t		m_dataIndex;
};
