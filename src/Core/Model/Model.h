#pragma once

#include "Core/Model/ModelTypes.h"
#include "Core/Model/ModelMorph.h"
#include "Core/Model/Bone/Node.h"
#include "Core/Model/Bone/IkSolver.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Chrivent {
	class Physics;
	class RigidBody;
	class Joint;

	// 한 재질이 그릴 인덱스 범위와 재질 번호를 보관한다.
	struct SubMesh {
		size_t	beginIndex = 0;
		size_t	indexCount = 0;
		size_t	materialId = 0;
	};

	// PMX 버텍스의 본 인덱스, 가중치와 SDEF 보조 데이터를 보관한다.
	struct Vertex {
		WeightType	weightType = WeightType::BoneDeform1;
		int32_t		boneIndices[4]{};
		float		boneWeights[4]{1};
		glm::vec3	sphericalDeformC = glm::vec3(0);
		glm::vec3	sphericalDeformR0 = glm::vec3(0);
		glm::vec3	sphericalDeformR1 = glm::vec3(0);
	};

	// 모프의 이름, 형식과 형식별 데이터 인덱스를 보관한다.
	struct Morph {
		std::string	name;
		float		weight = 0;
		float		saveAnimWeight = 0;
		MorphType	morphType = MorphType::Group;
		size_t		dataIndex = 0;
	};

	// 모델 재질의 색상, 텍스처와 렌더링 플래그를 보관한다.
	struct Material {
		glm::vec4				diffuse = glm::vec4(1);
		glm::vec3				specular = glm::vec3(0);
		float					specularPower = 1;
		glm::vec3				ambient = glm::vec3(0.2f);
		uint8_t					edgeFlag = 0;
		float					edgeSize = 0;
		glm::vec4				edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		std::filesystem::path	texture;
		std::filesystem::path	spTexture;
		SphereMode				spTextureMode = SphereMode::None;
		std::filesystem::path	toonTexture;
		glm::vec4				textureMulFactor = glm::vec4(1);
		glm::vec4				sphereTextureMulFactor = glm::vec4(1);
		glm::vec4				toonTextureMulFactor = glm::vec4(1);
		glm::vec4				textureAddFactor = glm::vec4(0);
		glm::vec4				sphereTextureAddFactor = glm::vec4(0);
		glm::vec4				toonTextureAddFactor = glm::vec4(0);
		bool					bothFace = false;
		bool					groundShadow = true;
		bool					shadowCaster = true;
		bool					shadowReceiver = true;
	};

	// 병렬 버텍스 갱신 작업이 처리할 연속 범위를 보관한다.
	struct UpdateRange {
		size_t vertexOffset = 0;
		size_t vertexCount = 0;
	};
	
	// 모델 패널에 표시할 본과 모프 그룹을 보관한다.
	struct ModelDisplayFrame {
		std::string				name;
		std::vector<uint32_t>	boneIndices;
		std::vector<uint32_t>	morphIndices;
	};

	// 모델 이름과 설명 같은 메타데이터를 보관한다.
	struct ModelInfoData {
		std::string									modelName;
		std::string									englishModelName;
		std::string									comment;
		std::string									englishComment;
	};

	// 원본 및 변형된 버텍스와 인덱스 데이터를 관리한다.
	struct ModelGeometryData {
		std::vector<glm::vec3>						positions;
		std::vector<glm::vec3>						normals;
		std::vector<glm::vec2>						uvs;
		std::vector<Vertex>							vertexBoneInfos;
		std::vector<char>							indices;
		size_t										indexCount = 0;
		size_t										indexElementSize = 0;
		glm::vec3									bboxMin = glm::vec3(0);
		glm::vec3									bboxMax = glm::vec3(0);
		std::vector<glm::vec3>						updatePositions;
		std::vector<glm::vec3>						updateNormals;
		std::vector<glm::vec2>						updateUVs;
		std::vector<glm::vec3>						previousPositions;
		std::vector<UpdateRange>					updateRanges;
	};

	// 모델 재질, 서브메시와 모프용 재질 상태를 관리한다.
	struct ModelMaterialData {
		std::vector<Material>						materials;
		std::vector<SubMesh>						subMeshes;
		std::vector<Material>						initMaterials;
		std::vector<MaterialMorph>					mulMaterialFactors;
		std::vector<MaterialMorph>					addMaterialFactors;
	};

	// 본 계층, IK와 스키닝 변환 행렬을 관리한다.
	class ModelSkeletonData {
		std::vector<std::unique_ptr<Node>>			nodes;
		std::vector<std::unique_ptr<IkSolver>>		ikSolvers;

	public:
		std::vector<glm::mat4>						transforms;
		std::vector<std::reference_wrapper<Node>>	sortedNodes;
		std::vector<ModelDisplayFrame>				displayFrames;

		const std::vector<std::unique_ptr<Node>>& GetNodes() const { return nodes; }
		const std::vector<std::unique_ptr<IkSolver>>& GetIkSolvers() const { return ikSolvers; }

		// 예상 개수만큼 노드 소유 목록의 메모리를 미리 확보한다.
		void ReserveNodes(const size_t count) { nodes.reserve(count); }
		// 모델이 소유할 노드를 목록 끝에 추가한다.
		void AddNode(std::unique_ptr<Node> node) { nodes.emplace_back(std::move(node)); }
		// 모델이 소유할 IK 솔버를 목록 끝에 추가한다.
		void AddIkSolver(std::unique_ptr<IkSolver> ikSolver) { ikSolvers.emplace_back(std::move(ikSolver)); }
	};

	// 형식별 모프 정의와 현재 누적 변형값을 관리한다.
	class ModelMorphData {
		std::vector<std::unique_ptr<Morph>>			morphs;

	public:
		std::vector<std::vector<PositionMorph>>		positionMorphs;
		std::vector<std::vector<UvMorph>>			uvMorphs;
		std::vector<std::vector<MaterialMorph>>		materialMorphs;
		std::vector<std::vector<BoneMorph>>			boneMorphs;
		std::vector<std::vector<GroupMorph>>		groupMorphs;
		std::vector<glm::vec3>						morphPositions;
		std::vector<glm::vec4>						morphUVs;

		const std::vector<std::unique_ptr<Morph>>& GetMorphs() const { return morphs; }

		// 모델이 소유할 모프를 목록 끝에 추가한다.
		void AddMorph(std::unique_ptr<Morph> morph) { morphs.emplace_back(std::move(morph)); }
	};

	// 모델의 Bullet 물리 월드, 강체와 조인트를 함께 소유하고 등록 수명을 관리한다.
	class ModelPhysicsData {
		std::unique_ptr<Physics> physics;
		std::vector<std::unique_ptr<RigidBody>> rigidBodies;
		std::vector<std::unique_ptr<Joint>> joints;

		// 강체와 조인트 정의가 Bullet 생성 조건과 모델 참조 범위를 만족하는지 검증한다.
		static std::expected<void, PhysicsError> ValidateDefinitions(
			const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
			const std::vector<JointDefinition>& jointDefinitions, std::size_t nodeCount);
		// 강체 변환을 본에 반영하고 루트 노드의 글로벌 변환을 갱신한다.
		void ReflectTransforms(const std::vector<std::unique_ptr<Node>>& nodes) const;

	public:
		ModelPhysicsData();
		~ModelPhysicsData();

		bool IsInitialized() const { return physics != nullptr; }

		// 두 모델 물리 상태의 전체 소유 리소스를 교환한다.
		void Swap(ModelPhysicsData& other);
		// 런타임 정의와 모델 노드로 물리 월드, 강체와 조인트를 구성한다.
		std::expected<void, PhysicsError> Initialize(const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
			const std::vector<JointDefinition>& jointDefinitions, const std::vector<std::unique_ptr<Node>>& nodes);
		// 현재 모델 포즈 기준으로 강체 상태와 충돌 쌍을 초기화한다.
		void ResetSimulation(const std::vector<std::unique_ptr<Node>>& nodes) const;
		// 지정한 경과 시간만큼 물리를 진행하고 강체 변환을 본에 반영한다.
		void UpdateSimulation(float elapsed, const std::vector<std::unique_ptr<Node>>& nodes) const;
		// 물리 월드에서 조인트와 강체를 제거하고 소유 리소스를 해제한다.
		void Reset();
	};

	// PMX 모델의 정보, 형상, 재질, 골격, 모프와 물리를 소유한다.
	class Model {
		ModelMorph morphEvaluator;
		ModelPhysicsData physicsData;
		uint64_t structureRevision = 0;

	public:
		ModelInfoData		infoData;
		ModelGeometryData	geometryData;
		ModelMaterialData	materialData;
		ModelSkeletonData	skeletonData;
		ModelMorphData		morphData;
		
		Model();
		~Model();

		uint64_t GetStructureRevision() const { return structureRevision; }

		// 모델에 활성화된 물리 월드가 있는지 확인한다.
		bool HasPhysics() const;
		// 모델이 소유한 리소스와 런타임 상태를 해제한다.
		void Reset();
		// 두 모델의 전체 소유 상태를 교환한다.
		void Swap(Model& other);
		// 현재 모프 가중치를 준비된 프레임의 형상, 재질과 본 상태에 누적한다.
		void AccumulateMorphs();
		// 런타임 정의 목록으로 모델 물리 월드, 강체와 조인트를 구성한다.
		std::expected<void, PhysicsError> InitializePhysics(const std::vector<RigidBodyDefinition>& rigidBodies,
			const std::vector<JointDefinition>& joints);
		// 강체와 조인트를 현재 모델 포즈 기준으로 초기화한다.
		void ResetPhysics();
		// 물리 시뮬레이션을 진행하고 강체 변환을 노드에 반영한다.
		void UpdatePhysics(float elapsed);
	};
}
