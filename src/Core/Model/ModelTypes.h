#pragma once

#include <cstdint>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
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

	enum class RigidBodyOperation : uint8_t {
		Static,
		Dynamic,
		DynamicAndBoneMerge
	};

	enum class RigidBodyShape : uint8_t {
		Sphere,
		Box,
		Capsule
	};

	// 모델 강체 생성에 필요한 연결 본, 형상, 물성과 충돌 설정을 보관한다.
	struct RigidBodyDefinition {
		int32_t nodeIndex = -1;
		RigidBodyShape shape = RigidBodyShape::Sphere;
		glm::vec3 shapeSize = glm::vec3(0);
		glm::vec3 translate = glm::vec3(0);
		glm::vec3 rotate = glm::vec3(0);
		float mass = 0;
		float translateDamping = 0;
		float rotateDamping = 0;
		float restitution = 0;
		float friction = 0;
		RigidBodyOperation operation = RigidBodyOperation::Static;
		uint16_t group = 0;
		uint16_t groupMask = 0;
	};

	// 모델 조인트 생성에 필요한 연결 강체, 제한과 스프링 계수를 보관한다.
	struct JointDefinition {
		int32_t rigidBodyAIndex = -1;
		int32_t rigidBodyBIndex = -1;
		glm::vec3 translate = glm::vec3(0);
		glm::vec3 rotate = glm::vec3(0);
		glm::vec3 translateLowerLimit = glm::vec3(0);
		glm::vec3 translateUpperLimit = glm::vec3(0);
		glm::vec3 rotateLowerLimit = glm::vec3(0);
		glm::vec3 rotateUpperLimit = glm::vec3(0);
		glm::vec3 springTranslateFactor = glm::vec3(0);
		glm::vec3 springRotateFactor = glm::vec3(0);
	};

	// 버텍스 하나의 위치 모프 오프셋을 보관한다.
	struct PositionMorph {
		int32_t vertexIndex;
		glm::vec3 position;
	};

	// 버텍스 하나의 UV 모프 오프셋을 보관한다.
	struct UvMorph {
		int32_t vertexIndex;
		glm::vec4 uv;
	};

	// 본 하나의 이동 및 회전 모프 값을 보관한다.
	struct BoneMorph {
		int32_t boneIndex;
		glm::vec3 position;
		glm::quat quaternion;
	};

	// 재질 하나에 적용할 색상 및 텍스처 계수 모프를 보관한다.
	struct MaterialMorph {
		int32_t materialIndex;
		OpType opType;
		glm::vec4 diffuse;
		glm::vec3 specular;
		float specularPower;
		glm::vec3 ambient;
		glm::vec4 edgeColor;
		float edgeSize;
		glm::vec4 textureFactor;
		glm::vec4 sphereTextureFactor;
		glm::vec4 toonTextureFactor;
	};

	// 그룹이 참조하는 하위 모프와 가중치를 보관한다.
	struct GroupMorph {
		int32_t morphIndex;
		float weight;
	};
}
