#pragma once

#include <future>

#include "../Node.h"
#include "../IkSolver.h"
#include "../Physics.h"

namespace Chrivent {
	struct GroupMorph;
	struct BoneMorph;
	struct UvMorph;
	struct PositionMorph;
	struct MaterialMorph;
	struct Animation;
	class RigidBody;
	class Joint;

	enum class SphereMode : uint8_t;
	enum class MorphType : uint8_t;
	enum class WeightType : uint8_t;

	struct SubMesh {
		int	beginIndex;
		int	indexCount;
		int	materialId;
	};

	struct Vertex {
		WeightType	weightType;
		int32_t		boneIndices[4];
		float		boneWeights[4];
		glm::vec3	sphericalDeformC;
		glm::vec3	sphericalDeformR0;
		glm::vec3	sphericalDeformR1;
	};

	struct Morph {
		std::string	name;
		float		weight = 0;
		float		saveAnimWeight = 0;
		MorphType	morphType;
		size_t		dataIndex = 0;
	};

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
		std::filesystem::path	cartoonTexture;
		glm::vec4				textureMulFactor = glm::vec4(1);
		glm::vec4				sphereTextureMulFactor = glm::vec4(1);
		glm::vec4				cartoonTextureMulFactor = glm::vec4(1);
		glm::vec4				textureAddFactor = glm::vec4(0);
		glm::vec4				sphereTextureAddFactor = glm::vec4(0);
		glm::vec4				cartoonTextureAddFactor = glm::vec4(0);
		bool					bothFace = false;
		bool					groundShadow = true;
		bool					shadowCaster = true;
		bool					shadowReceiver = true;
	};

	struct UpdateRange {
		size_t vertexOffset;
		size_t vertexCount;
	};

	struct ModelInfoData {
		std::string									modelName;
		std::string									englishModelName;
		std::string									comment;
		std::string									englishComment;
	};

	struct ModelGeometryData {
		std::vector<glm::vec3>						positions;
		std::vector<glm::vec3>						normals;
		std::vector<glm::vec2>						uvs;
		std::vector<Vertex>							vertexBoneInfos;
		std::vector<char>							indices;
		size_t										indexCount = 0;
		size_t										indexElementSize = 0;
		glm::vec3									bboxMin;
		glm::vec3									bboxMax;
		std::vector<glm::vec3>						updatePositions;
		std::vector<glm::vec3>						updateNormals;
		std::vector<glm::vec2>						updateUVs;
		uint32_t									parallelUpdateCount = 0;
		std::vector<UpdateRange>					updateRanges;
		std::vector<std::future<void>>				parallelUpdateFutures;
	};

	struct ModelMaterialData {
		std::vector<Material>						materials;
		std::vector<SubMesh>						subMeshes;
		std::vector<Material>						initMaterials;
		std::vector<MaterialMorph>					mulMaterialFactors;
		std::vector<MaterialMorph>					addMaterialFactors;
	};

	struct ModelSkeletonData {
		std::vector<std::shared_ptr<Node>>			nodes;
		std::vector<std::shared_ptr<IkSolver>>		ikSolvers;
		std::vector<glm::mat4>						transforms;
		std::vector<std::reference_wrapper<Node>>	sortedNodes;
	};

	struct ModelMorphData {
		std::vector<std::unique_ptr<Morph>>			morphs;
		std::vector<std::vector<PositionMorph>>		positionMorphs;
		std::vector<std::vector<UvMorph>>			uvMorphs;
		std::vector<std::vector<MaterialMorph>>		materialMorphs;
		std::vector<std::vector<BoneMorph>>			boneMorphs;
		std::vector<std::vector<GroupMorph>>		groupMorphs;
		std::vector<glm::vec3>						morphPositions;
		std::vector<glm::vec4>						morphUVs;
	};

	struct ModelPhysicsData {
		std::unique_ptr<Physics>					physics;
		std::vector<std::unique_ptr<RigidBody>>		rigidBodies;
		std::vector<std::unique_ptr<Joint>>			joints;
	};

	struct Model {
		ModelInfoData		infoData;
		ModelGeometryData	geometryData;
		ModelMaterialData	materialData;
		ModelSkeletonData	skeletonData;
		ModelMorphData		morphData;
		ModelPhysicsData	physicsData;
		
		~Model();

		// 모델이 소유한 리소스와 런타임 상태를 해제한다.
		void Destroy();
	};
}
