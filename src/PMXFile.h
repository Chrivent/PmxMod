#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>

enum class EncodeType : uint8_t {
	UTF16,
	UTF8
};

enum class WeightType : uint8_t {
	BDEF1,
	BDEF2,
	BDEF4,
	SDEF,
	QDEF
};

enum class DrawModeFlags : uint8_t {
	BothFace = 0x01,
	GroundShadow = 0x02,
	CastSelfShadow = 0x04,
	ReceiveSelfShadow = 0x08,
	DrawEdge = 0x10,
	VertexColor = 0x20,
	DrawPoint = 0x40,
	DrawLine = 0x80
};

enum class SphereMode : uint8_t {
	None,
	Mul,
	Add,
	SubTexture
};

enum class ToonMode : uint8_t {
	Separate,
	Common
};

enum class BoneFlags : uint16_t {
	TargetShowMode = 0x0001,
	AllowRotate = 0x0002,
	AllowTranslate = 0x0004,
	Visible = 0x0008,
	AllowControl = 0x0010,
	IK = 0x0020,
	AppendLocal = 0x0080,
	AppendRotate = 0x0100,
	AppendTranslate = 0x0200,
	FixedAxis = 0x0400,
	LocalAxis = 0x800,
	DeformAfterPhysics = 0x1000,
	DeformOuterParent = 0x2000
};

enum class MorphType : uint8_t {
	Group,
	Position,
	Bone,
	UV,
	AddUV1,
	AddUV2,
	AddUV3,
	AddUV4,
	Material,
	Flip,
	Impulse
};

enum class OpType : uint8_t {
	Mul,
	Add
};

enum class ControlPanel : uint8_t {
	SystemReserved,
	Brow,
	Eye,
	Mouth,
	Other
};

enum class TargetType : uint8_t {
	BoneIndex,
	MorphIndex
};

enum class FrameType : uint8_t {
	DefaultFrame,
	SpecialFrame
};

enum class Operation : uint8_t {
	Static,
	Dynamic,
	DynamicAndBoneMerge
};

enum class Shape : uint8_t {
	Sphere,
	Box,
	Capsule,
};

enum class JointType : uint8_t {
	SpringDOF6,
	DOF6,
	P2P,
	ConeTwist,
	Slider,
	Hinge,
};

enum class SoftBodyType : uint8_t {
	TriMesh,
	Rope,
};

enum class SoftBodyMask : uint8_t {
	BLink = 0x01,
	Cluster = 0x02,
	HybridLink = 0x04,
};

enum class AeroModel : int32_t {
	kAeroModelV_TwoSided,
	kAeroModelV_OneSided,
	kAeroModelF_TwoSided,
	kAeroModelF_OneSided,
};

struct PositionMorph {
	int32_t		m_vertexIndex;
	glm::vec3	m_position;
};

struct UVMorph {
	int32_t		m_vertexIndex;
	glm::vec4	m_uv;
};

struct BoneMorph {
	int32_t		m_boneIndex;
	glm::vec3	m_position;
	glm::quat	m_quaternion;
};

struct MaterialMorph {
	int32_t		m_materialIndex;
	OpType		m_opType;
	glm::vec4	m_diffuse;
	glm::vec3	m_specular;
	float		m_specularPower;
	glm::vec3	m_ambient;
	glm::vec4	m_edgeColor;
	float		m_edgeSize;
	glm::vec4	m_textureFactor;
	glm::vec4	m_sphereTextureFactor;
	glm::vec4	m_toonTextureFactor;
};

struct GroupMorph {
	int32_t	m_morphIndex;
	float	m_weight;
};

struct FlipMorph {
	int32_t	m_morphIndex;
	float	m_weight;
};

struct ImpulseMorph {
	int32_t		m_rigidbodyIndex;
	uint8_t		m_localFlag;
	glm::vec3	m_translateVelocity;
	glm::vec3	m_rotateTorque;
};

struct Target {
	TargetType	m_type;
	int32_t		m_index;
};

struct AnchorRigidbody {
	int32_t		m_rigidBodyIndex;
	int32_t		m_vertexIndex;
	uint8_t		m_nearMode;
};

struct PMXHeader {
	char		m_magic[4];
	float		m_version;
	uint8_t		m_dataSize;
	EncodeType	m_encodeType;
	uint8_t		m_addUVNum;
	uint8_t		m_vertexIndexSize;
	uint8_t		m_textureIndexSize;
	uint8_t		m_materialIndexSize;
	uint8_t		m_boneIndexSize;
	uint8_t		m_morphIndexSize;
	uint8_t		m_rigidbodyIndexSize;
};

struct PMXInfo {
	std::string	m_modelName;
	std::string	m_englishModelName;
	std::string	m_comment;
	std::string	m_englishComment;
};

struct PMXVertex {
	glm::vec3		m_position;
	glm::vec3		m_normal;
	glm::vec2		m_uv;
	glm::vec4		m_addUV[4];
	WeightType		m_weightType;
	int32_t			m_boneIndices[4];
	float			m_boneWeights[4];
	glm::vec3		m_sdefC;
	glm::vec3		m_sdefR0;
	glm::vec3		m_sdefR1;
	float			m_edgeMag;
};

struct PMXFace {
	uint32_t	m_vertices[3];
};

struct PMXTexture {
	std::filesystem::path m_textureName;
};

struct PMXMaterial {
	std::string	m_name;
	std::string	m_englishName;
	glm::vec4	m_diffuse;
	glm::vec3	m_specular;
	float		m_specularPower;
	glm::vec3	m_ambient;
	DrawModeFlags m_drawMode;
	glm::vec4	m_edgeColor;
	float		m_edgeSize;
	int32_t	m_textureIndex;
	int32_t	m_sphereTextureIndex;
	SphereMode m_sphereMode;
	ToonMode	m_toonMode;
	int32_t		m_toonTextureIndex;
	std::string	m_memo;
	int32_t	m_numFaceVertices;
};

struct PMXIKLink {
	int32_t			m_ikBoneIndex;
	unsigned char	m_enableLimit;
	glm::vec3	m_limitMin;
	glm::vec3	m_limitMax;
};

struct PMXBone {
	std::string	m_name;
	std::string	m_englishName;
	glm::vec3	m_position;
	int32_t		m_parentBoneIndex;
	int32_t		m_deformDepth;
	BoneFlags	m_boneFlag;
	glm::vec3	m_positionOffset;
	int32_t		m_linkBoneIndex;
	int32_t	m_appendBoneIndex;
	float	m_appendWeight;
	glm::vec3	m_fixedAxis;
	glm::vec3	m_localXAxis;
	glm::vec3	m_localZAxis;
	int32_t	m_keyValue;
	int32_t	m_ikTargetBoneIndex;
	int32_t	m_ikIterationCount;
	float	m_ikLimit;
	std::vector<PMXIKLink>	m_ikLinks;
};

struct PMXMorph {
	std::string		m_name;
	std::string		m_englishName;
	ControlPanel	m_controlPanel;
	MorphType		m_morphType;
	std::vector<PositionMorph>	m_positionMorph;
	std::vector<UVMorph>		m_uvMorph;
	std::vector<BoneMorph>		m_boneMorph;
	std::vector<MaterialMorph>	m_materialMorph;
	std::vector<GroupMorph>		m_groupMorph;
	std::vector<FlipMorph>		m_flipMorph;
	std::vector<ImpulseMorph>	m_impulseMorph;
};

struct PMXDisplayFrame {
	std::string	m_name;
	std::string	m_englishName;
	FrameType			m_flag;
	std::vector<Target>	m_targets;
};

struct PMXRigidbody {
	std::string	m_name;
	std::string	m_englishName;
	int32_t		m_boneIndex;
	uint8_t		m_group;
	uint16_t	m_collisionGroup;
	Shape		m_shape;
	glm::vec3	m_shapeSize;
	glm::vec3	m_translate;
	glm::vec3	m_rotate;
	float	m_mass;
	float	m_translateDimmer;
	float	m_rotateDimmer;
	float	m_repulsion;
	float	m_friction;
	Operation	m_op;
};

struct PMXJoint {
	std::string	m_name;
	std::string	m_englishName;
	JointType	m_type;
	int32_t		m_rigidbodyAIndex;
	int32_t		m_rigidbodyBIndex;
	glm::vec3	m_translate;
	glm::vec3	m_rotate;
	glm::vec3	m_translateLowerLimit;
	glm::vec3	m_translateUpperLimit;
	glm::vec3	m_rotateLowerLimit;
	glm::vec3	m_rotateUpperLimit;
	glm::vec3	m_springTranslateFactor;
	glm::vec3	m_springRotateFactor;
};

struct PMXSoftBody {
	std::string	m_name;
	std::string	m_englishName;
	SoftBodyType	m_type;
	int32_t			m_materialIndex;
	uint8_t		m_group;
	uint16_t	m_collisionGroup;
	SoftBodyMask	m_flag;
	int32_t	m_BLinkLength;
	int32_t	m_numClusters;
	float	m_totalMass;
	float	m_collisionMargin;
	int32_t		m_aeroModel;
	float	m_VCF;
	float	m_DP;
	float	m_DG;
	float	m_LF;
	float	m_PR;
	float	m_VC;
	float	m_DF;
	float	m_MT;
	float	m_CHR;
	float	m_KHR;
	float	m_SHR;
	float	m_AHR;
	float	m_SRHR_CL;
	float	m_SKHR_CL;
	float	m_SSHR_CL;
	float	m_SR_SPLT_CL;
	float	m_SK_SPLT_CL;
	float	m_SS_SPLT_CL;
	int32_t	m_V_IT;
	int32_t	m_P_IT;
	int32_t	m_D_IT;
	int32_t	m_C_IT;
	float	m_LST;
	float	m_AST;
	float	m_VST;
	std::vector<AnchorRigidbody>	m_anchorRigidBodies;
	std::vector<int32_t>	m_pinVertexIndices;
};

struct PMXFile {
	PMXHeader						m_header;
	PMXInfo							m_info;
	std::vector<PMXVertex>			m_vertices;
	std::vector<PMXFace>			m_faces;
	std::vector<PMXTexture>			m_textures;
	std::vector<PMXMaterial>		m_materials;
	std::vector<PMXBone>			m_bones;
	std::vector<PMXMorph>			m_morphs;
	std::vector<PMXDisplayFrame>	m_displayFrames;
	std::vector<PMXRigidbody>		m_rigidBodies;
	std::vector<PMXJoint>			m_joints;
	std::vector<PMXSoftBody>		m_softbodies;
};

bool ReadPMXFile(PMXFile* pmxFile, const std::filesystem::path& filename);
