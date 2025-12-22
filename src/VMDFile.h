#pragma once

#include <cstdint>
#include <vector>
#include <filesystem>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

enum class ShadowType : uint8_t {
	Off,
	Mode1,
	Mode2
};

struct VMDHeader {
	char m_header[30];
	char m_modelName[20];
};

struct VMDMotion {
	char			m_boneName[15];
	uint32_t		m_frame;
	glm::vec3		m_translate;
	glm::quat		m_quaternion;
	uint8_t			m_interpolation[64];
};

struct VMDMorph {
	char			m_blendShapeName[15];
	uint32_t		m_frame{};
	float			m_weight{};
};

struct VMDCamera {
	uint32_t		m_frame;
	float			m_distance;
	glm::vec3		m_interest;
	glm::vec3		m_rotate;
	uint8_t			m_interpolation[24];
	uint32_t		m_viewAngle;
	uint8_t			m_isPerspective;
};

struct VMDLight {
	uint32_t	m_frame;
	glm::vec3	m_color;
	glm::vec3	m_position;
};

struct VMDShadow {
	uint32_t	m_frame;
	ShadowType	m_shadowType;
	float		m_distance;
};

struct VMDIkInfo {
	char			m_name[20];
	uint8_t			m_enable{};
};

struct VMDIk {
	uint32_t	m_frame;
	uint8_t		m_show;
	std::vector<VMDIkInfo>	m_ikInfos;
};

struct VMDFile {
	VMDHeader					m_header;
	std::vector<VMDMotion>		m_motions;
	std::vector<VMDMorph>		m_morphs;
	std::vector<VMDCamera>		m_cameras;
	std::vector<VMDLight>		m_lights;
	std::vector<VMDShadow>		m_shadows;
	std::vector<VMDIk>			m_iks;
};

bool ReadVMDFile(VMDFile* vmd, const std::filesystem::path& filename);
