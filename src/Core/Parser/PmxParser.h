#pragma once

#include "Core/Parser/BinaryReader.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	enum class EncodeType : uint8_t {
		Utf16,
		Utf8
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
		Ik = 0x0020,
		AppendLocal = 0x0080,
		AppendRotate = 0x0100,
		AppendTranslate = 0x0200,
		FixedAxis = 0x0400,
		LocalAxis = 0x800,
		DeformAfterPhysics = 0x1000,
		DeformOuterParent = 0x2000
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
		KAeroModelVTwoSided,
		KAeroModelVOneSided,
		KAeroModelFTwoSided,
		KAeroModelFOneSided,
	};
	
	enum class WeightType : uint8_t {
		BoneDeform1,
		BoneDeform2,
		BoneDeform4,
		SphericalDeform,
		QuaternionDeform
	};

	enum class SphereMode : uint8_t {
		None,
		Mul,
		Add,
		SubTexture
	};

	enum class MorphType : uint8_t {
		Group,
		Position,
		Bone,
		Uv,
		AddUv1,
		AddUv2,
		AddUv3,
		AddUv4,
		Material,
		Flip,
		Impulse
	};

	enum class OpType : uint8_t {
		Mul,
		Add
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
		SpringDof6,
		Dof6,
		P2P,
		ConeTwist,
		Slider,
		Hinge,
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

	class PmxParser {
	public:
		struct PmxHeader {
			char		magic[4];
			float		version;
			uint8_t		dataSize;
			EncodeType	encodeType;
			uint8_t		addUvNum;
			uint8_t		vertexIndexSize;
			uint8_t		textureIndexSize;
			uint8_t		materialIndexSize;
			uint8_t		boneIndexSize;
			uint8_t		morphIndexSize;
			uint8_t		rigidbodyIndexSize;
		};

		struct PmxInfo {
			std::string	modelName;
			std::string	englishModelName;
			std::string	comment;
			std::string	englishComment;
		};

		struct PmxVertex {
			glm::vec3		position;
			glm::vec3		normal;
			glm::vec2		uv;
			glm::vec4		addUv[4];
			WeightType		weightType;
			int32_t			boneIndices[4];
			float			boneWeights[4];
			glm::vec3		sphericalDeformC;
			glm::vec3		sphericalDeformR0;
			glm::vec3		sphericalDeformR1;
			float			edgeMag;
		};

		struct PmxFace {
			uint32_t	vertices[3];
		};

		struct PmxTexture {
			std::filesystem::path textureName;
		};

		struct PmxMaterial {
			std::string		name;
			std::string		englishName;
			glm::vec4		diffuse;
			glm::vec3		specular;
			float			specularPower;
			glm::vec3		ambient;
			DrawModeFlags	drawMode;
			glm::vec4		edgeColor;
			float			edgeSize;
			int32_t			textureIndex;
			int32_t			sphereTextureIndex;
			SphereMode		sphereMode;
			ToonMode		toonMode;
			int32_t			toonTextureIndex;
			std::string		memo;
			int32_t			numFaceVertices;
		};

		struct PmxIkLink {
			int32_t		ikBoneIndex;
			uint8_t		enableLimit;
			glm::vec3	limitMin;
			glm::vec3	limitMax;
		};

		struct PmxBone {
			std::string				name;
			std::string				englishName;
			glm::vec3				position;
			int32_t					parentBoneIndex;
			int32_t					deformDepth;
			BoneFlags				boneFlag;
			glm::vec3				positionOffset;
			int32_t					linkBoneIndex;
			int32_t					appendBoneIndex;
			float					appendWeight;
			glm::vec3				fixedAxis;
			glm::vec3				localXAxis;
			glm::vec3				localZAxis;
			int32_t					keyValue;
			int32_t					ikTargetBoneIndex;
			int32_t					ikIterationCount;
			float					ikLimit;
			std::vector<PmxIkLink>	ikLinks;
		};

		struct PmxMorph {
			std::string					name;
			std::string					englishName;
			ControlPanel				controlPanel;
			MorphType					morphType;
			std::vector<PositionMorph>	positionMorph;
			std::vector<UvMorph>		uvMorph;
			std::vector<BoneMorph>		boneMorph;
			std::vector<MaterialMorph>	materialMorph;
			std::vector<GroupMorph>		groupMorph;
			std::vector<FlipMorph>		flipMorph;
			std::vector<ImpulseMorph>	impulseMorph;
		};

		struct PmxDisplayFrame {
			std::string			name;
			std::string			englishName;
			FrameType			flag;
			std::vector<Target>	targets;
		};

		struct PmxRigidbody {
			std::string	name;
			std::string	englishName;
			int32_t		boneIndex;
			uint8_t		group;
			uint16_t	collisionGroup;
			Shape		shape;
			glm::vec3	shapeSize;
			glm::vec3	translate;
			glm::vec3	rotate;
			float		mass;
			float		translateDimmer;
			float		rotateDimmer;
			float		repulsion;
			float		friction;
			Operation	op;
		};

		struct PmxJoint {
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

		struct PmxSoftBody {
			std::string						name;
			std::string						englishName;
			SoftBodyType					type;
			int32_t							materialIndex;
			uint8_t							group;
			uint16_t						collisionGroup;
			SoftBodyMask					flag;
			int32_t							bodyLinkLength;
			int32_t							numClusters;
			float							totalMass;
			float							collisionMargin;
			int32_t							aeroModel;
			float							vcf;
			float							dp;
			float							dg;
			float							lf;
			float							pr;
			float							vc;
			float							df;
			float							mt;
			float							chr;
			float							khr;
			float							shr;
			float							ahr;
			float							sRhrCl;
			float							sKhrCl;
			float							sShrCl;
			float							srSplitCl;
			float							skSplitCl;
			float							ssSplitCl;
			int32_t							vIt;
			int32_t							pIt;
			int32_t							dIt;
			int32_t							cIt;
			float							lst;
			float							ast;
			float							vst;
			std::vector<AnchorRigidbody>	anchorRigidBodies;
			std::vector<int32_t>			pinVertexIndices;
		};

		struct PmxData {
			PmxHeader						header;
			PmxInfo							info;
			std::vector<PmxVertex>			vertices;
			std::vector<PmxFace>			faces;
			std::vector<PmxTexture>			textures;
			std::vector<PmxMaterial>		materials;
			std::vector<PmxBone>			bones;
			std::vector<PmxMorph>			morphs;
			std::vector<PmxDisplayFrame>	displayFrames;
			std::vector<PmxRigidbody>		rigidBodies;
			std::vector<PmxJoint>			joints;
			std::vector<PmxSoftBody>		softBodies;
		};

	private:
		PmxData data{};

		// 현재 PMX 인코딩 설정에 맞춰 문자열을 읽는다.
		void ReadString(BinaryReader& reader, std::string* val) const;
		// PMX 헤더와 인덱스 크기 정보를 읽는다.
		void ReadHeader(BinaryReader& reader);
		// 모델 이름과 설명 정보를 읽는다.
		void ReadInfo(BinaryReader& reader);
		// 버텍스 목록과 스키닝 정보를 읽는다.
		void ReadVertex(BinaryReader& reader);
		// 면 인덱스 목록을 읽는다.
		void ReadFace(BinaryReader& reader);
		// 텍스처 경로 목록을 읽는다.
		void ReadTexture(BinaryReader& reader);
		// 재질 목록과 렌더링 속성을 읽는다.
		void ReadMaterial(BinaryReader& reader);
		// 본 계층과 IK 정보를 읽는다.
		void ReadBone(BinaryReader& reader);
		// 모프 목록과 모프별 데이터를 읽는다.
		void ReadMorph(BinaryReader& reader);
		// 표시 프레임 정보를 읽는다.
		void ReadDisplayFrame(BinaryReader& reader);
		// 강체 정보를 읽는다.
		void ReadRigidbody(BinaryReader& reader);
		// 조인트 제약 정보를 읽는다.
		void ReadJoint(BinaryReader& reader);
		// 소프트바디 정보를 읽는다.
		void ReadSoftBody(BinaryReader& reader);
		// 읽은 PMX 데이터의 인덱스와 상호 참조를 검증한다.
		void ValidateData(BinaryReader& reader) const;
		// 이전에 읽은 PMX 데이터를 초기화한다.
		void Clear();

	public:
		const PmxData& GetData() const { return data; }
		
		// PMX 파일 전체를 읽어 내부 데이터 구조에 저장한다.
		std::expected<void, ParseError> ReadFile(const std::filesystem::path& filename);
	};
}
