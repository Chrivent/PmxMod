#pragma once

#include <filesystem>
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

enum class ShadowType : uint8_t {
	Off,
	Mode1,
	Mode2
};

struct PositionMorph {
	int32_t		vertexIndex;
	glm::vec3	position;
};

struct UvMorph {
	int32_t		vertexIndex;
	glm::vec4	uv;
};

struct BoneMorph {
	int32_t		boneIndex;
	glm::vec3	position;
	glm::quat	quaternion;
};

struct MaterialMorph {
	int32_t		materialIndex;
	OpType		opType;
	glm::vec4	diffuse;
	glm::vec3	specular;
	float		specularPower;
	glm::vec3	ambient;
	glm::vec4	edgeColor;
	float		edgeSize;
	glm::vec4	textureFactor;
	glm::vec4	sphereTextureFactor;
	glm::vec4	toonTextureFactor;
};

struct GroupMorph {
	int32_t	morphIndex;
	float	weight;
};

struct FlipMorph {
	int32_t	morphIndex;
	float	weight;
};

struct ImpulseMorph {
	int32_t		rigidbodyIndex;
	uint8_t		localFlag;
	glm::vec3	translateVelocity;
	glm::vec3	rotateTorque;
};

struct Target {
	TargetType	type;
	int32_t		index;
};

struct AnchorRigidbody {
	int32_t		rigidBodyIndex;
	int32_t		vertexIndex;
	uint8_t		nearMode;
};

class PMXReader {
	struct PMXHeader {
		char		magic[4];
		float		version;
		uint8_t		dataSize;
		EncodeType	encodeType;
		uint8_t		addUVNum;
		uint8_t		vertexIndexSize;
		uint8_t		textureIndexSize;
		uint8_t		materialIndexSize;
		uint8_t		boneIndexSize;
		uint8_t		morphIndexSize;
		uint8_t		rigidbodyIndexSize;
	};

	struct PMXInfo {
		std::string	modelName;
		std::string	englishModelName;
		std::string	comment;
		std::string	englishComment;
	};

	struct PMXVertex {
		glm::vec3		position;
		glm::vec3		normal;
		glm::vec2		uv;
		glm::vec4		addUV[4];
		WeightType		weightType;
		int32_t			boneIndices[4];
		float			boneWeights[4];
		glm::vec3		sdefC;
		glm::vec3		sdefR0;
		glm::vec3		sdefR1;
		float			edgeMag;
	};

	struct PMXFace {
		uint32_t	vertices[3];
	};

	struct PMXTexture {
		std::filesystem::path textureName;
	};

	struct PMXMaterial {
		std::string	name;
		std::string	englishName;
		glm::vec4	diffuse;
		glm::vec3	specular;
		float		specularPower;
		glm::vec3	ambient;
		DrawModeFlags drawMode;
		glm::vec4	edgeColor;
		float		edgeSize;
		int32_t	textureIndex;
		int32_t	sphereTextureIndex;
		SphereMode sphereMode;
		ToonMode	toonMode;
		int32_t		toonTextureIndex;
		std::string	memo;
		int32_t	numFaceVertices;
	};

	struct PMXIKLink {
		int32_t			ikBoneIndex;
		unsigned char	enableLimit;
		glm::vec3	limitMin;
		glm::vec3	limitMax;
	};

	struct PMXBone {
		std::string	name;
		std::string	englishName;
		glm::vec3	position;
		int32_t		parentBoneIndex;
		int32_t		deformDepth;
		BoneFlags	boneFlag;
		glm::vec3	positionOffset;
		int32_t		linkBoneIndex;
		int32_t	appendBoneIndex;
		float	appendWeight;
		glm::vec3	fixedAxis;
		glm::vec3	localXAxis;
		glm::vec3	localZAxis;
		int32_t	keyValue;
		int32_t	ikTargetBoneIndex;
		int32_t	ikIterationCount;
		float	ikLimit;
		std::vector<PMXIKLink>	ikLinks;
	};

	struct PMXMorph {
		std::string		name;
		std::string		englishName;
		ControlPanel	controlPanel;
		MorphType		morphType;
		std::vector<PositionMorph>	positionMorph;
		std::vector<UvMorph>		uvMorph;
		std::vector<BoneMorph>		boneMorph;
		std::vector<MaterialMorph>	materialMorph;
		std::vector<GroupMorph>		groupMorph;
		std::vector<FlipMorph>		flipMorph;
		std::vector<ImpulseMorph>	impulseMorph;
	};

	struct PMXDisplayFrame {
		std::string	name;
		std::string	englishName;
		FrameType			flag;
		std::vector<Target>	targets;
	};

	/// 현재 PMX 인코딩 설정에 맞춰 문자열을 읽는다.
	void ReadString(std::istream& is, std::string* val) const;
	/// PMX 헤더와 인덱스 크기 정보를 읽는다.
	void ReadHeader(std::istream& is);
	/// 모델 이름과 설명 정보를 읽는다.
	void ReadInfo(std::istream& is);
	/// 버텍스 목록과 스키닝 정보를 읽는다.
	void ReadVertex(std::istream& is);
	/// 면 인덱스 목록을 읽는다.
	void ReadFace(std::istream& is);
	/// 텍스처 경로 목록을 읽는다.
	void ReadTexture(std::istream& is);
	/// 재질 목록과 렌더링 속성을 읽는다.
	void ReadMaterial(std::istream& is);
	/// 본 계층과 IK 정보를 읽는다.
	void ReadBone(std::istream& is);
	/// 모프 목록과 모프별 데이터를 읽는다.
	void ReadMorph(std::istream& is);
	/// 표시 프레임 정보를 읽는다.
	void ReadDisplayFrame(std::istream& is);
	/// 강체 정보를 읽는다.
	void ReadRigidbody(std::istream& is);
	/// 조인트 제약 정보를 읽는다.
	void ReadJoint(std::istream& is);
	/// 소프트바디 정보를 읽는다.
	void ReadSoftBody(std::istream& is);

public:
	struct PMXRigidbody {
		std::string	name;
		std::string	englishName;
		int32_t		boneIndex;
		uint8_t		group;
		uint16_t	collisionGroup;
		Shape		shape;
		glm::vec3	shapeSize;
		glm::vec3	translate;
		glm::vec3	rotate;
		float	mass;
		float	translateDimmer;
		float	rotateDimmer;
		float	repulsion;
		float	friction;
		Operation	op;
	};

	struct PMXJoint {
		std::string	name;
		std::string	englishName;
		JointType	type;
		int32_t		rigidbodyAIndex;
		int32_t		rigidbodyBIndex;
		glm::vec3	translate;
		glm::vec3	rotate;
		glm::vec3	translateLowerLimit;
		glm::vec3	translateUpperLimit;
		glm::vec3	rotateLowerLimit;
		glm::vec3	rotateUpperLimit;
		glm::vec3	springTranslateFactor;
		glm::vec3	springRotateFactor;
	};

	struct PMXSoftBody {
		std::string	name;
		std::string	englishName;
		SoftBodyType	type;
		int32_t			materialIndex;
		uint8_t		group;
		uint16_t	collisionGroup;
		SoftBodyMask	flag;
		int32_t	BLinkLength;
		int32_t	numClusters;
		float	totalMass;
		float	collisionMargin;
		int32_t		aeroModel;
		float	VCF;
		float	DP;
		float	DG;
		float	LF;
		float	PR;
		float	VC;
		float	DF;
		float	MT;
		float	CHR;
		float	KHR;
		float	SHR;
		float	AHR;
		float	SRHR_CL;
		float	SKHR_CL;
		float	SSHR_CL;
		float	SR_SPLT_CL;
		float	SK_SPLT_CL;
		float	SS_SPLT_CL;
		int32_t	V_IT;
		int32_t	P_IT;
		int32_t	D_IT;
		int32_t	C_IT;
		float	LST;
		float	AST;
		float	VST;
		std::vector<AnchorRigidbody>	anchorRigidBodies;
		std::vector<int32_t>	pinVertexIndices;
	};

	PMXHeader						header;
	PMXInfo							info;
	std::vector<PMXVertex>			vertices;
	std::vector<PMXFace>			faces;
	std::vector<PMXTexture>			textures;
	std::vector<PMXMaterial>		materials;
	std::vector<PMXBone>			bones;
	std::vector<PMXMorph>			morphs;
	std::vector<PMXDisplayFrame>	displayFrames;
	std::vector<PMXRigidbody>		rigidBodies;
	std::vector<PMXJoint>			joints;
	std::vector<PMXSoftBody>		softbodies;

	/// PMX 파일 전체를 읽어 내부 데이터 구조에 저장한다.
	bool ReadFile(const std::filesystem::path& filename);
};

class VMDReader {
	struct VMDHeader {
		char header[30];
		char modelName[20];
	};

	struct VMDMorph {
		char			blendShapeName[15]{};
		uint32_t		frame{};
		float			weight{};
	};

	struct VMDCamera {
		uint32_t		frame;
		float			distance;
		glm::vec3		interest;
		glm::vec3		rotate;
		uint8_t			interpolation[24];
		uint32_t		viewAngle;
		uint8_t			isPerspective;
	};

	struct VMDLight {
		uint32_t	frame;
		glm::vec3	color;
		glm::vec3	position;
	};

	struct VMDShadow {
		uint32_t	frame;
		ShadowType	shadowType;
		float		distance;
	};

	struct VMDIkInfo {
		char			name[20]{};
		uint8_t			enable{};
	};

	struct VMDIk {
		uint32_t	frame;
		uint8_t		show;
		std::vector<VMDIkInfo>	ikInfos;
	};

	/// VMD 헤더와 대상 모델 이름을 읽는다.
	void ReadHeader(std::istream& is);
	/// 본 모션 키프레임 목록을 읽는다.
	void ReadMotion(std::istream& is);
	/// 모프 키프레임 목록을 읽는다.
	void ReadBlendShape(std::istream& is);
	/// 카메라 키프레임 목록을 읽는다.
	void ReadCamera(std::istream& is);
	/// 라이트 키프레임 목록을 읽는다.
	void ReadLight(std::istream& is);
	/// 그림자 키프레임 목록을 읽는다.
	void ReadShadow(std::istream& is);
	/// IK 표시/활성화 키프레임 목록을 읽는다.
	void ReadIK(std::istream& is);

public:
	struct VMDMotion {
		char			boneName[15];
		uint32_t		frame;
		glm::vec3		translate;
		glm::quat		quaternion;
		uint8_t			interpolation[64];
	};

	VMDHeader					header;
	std::vector<VMDMotion>		motions;
	std::vector<VMDMorph>		morphs;
	std::vector<VMDCamera>		cameras;
	std::vector<VMDLight>		lights;
	std::vector<VMDShadow>		shadows;
	std::vector<VMDIk>			iks;

	/// VMD 파일 전체를 읽어 모션/카메라/모프 데이터를 저장한다.
	bool ReadFile(const std::filesystem::path& filename);
};
