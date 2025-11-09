#pragma once

#include <string>
#include <vector>
#include <memory>

enum class PMXMorphType : uint8_t;

class MMDMorph
{
public:
	MMDMorph();

	void SaveBaseAnimation();
	void LoadBaseAnimation();
	void ClearBaseAnimation();

	std::string	m_name;
	float		m_weight;
	float		m_saveAnimWeight;

	PMXMorphType	m_morphType;
	size_t		m_dataIndex;
};

class MMDMorphManager
{
public:
	size_t FindMorphIndex(const std::string& name);
	MMDMorph* GetMorph(size_t idx) const;
	MMDMorph* GetMorph(const std::string& name);
	MMDMorph* AddMorph();

	std::vector<std::unique_ptr<MMDMorph>>	m_morphs;
};
