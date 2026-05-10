#pragma once

#include <future>
#include <functional>

#include "Node.h"
#include "IkSolver.h"
#include "Physics.h"

struct GroupMorph;
struct BoneMorph;
struct UvMorph;
struct PositionMorph;
struct MaterialMorph;
class RigidBody;
class Joint;
class Animation;

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

class Model {
public:
	~Model();

	std::vector<glm::vec3>						positions;
	std::vector<glm::vec3>						updatePositions;
	std::vector<glm::vec3>						updateNormals;
	std::vector<glm::vec2>						updateUVs;
	std::vector<char>							indices;
	size_t										indexCount = 0;
	size_t										indexElementSize = 0;
	std::vector<Material>						materials;
	std::vector<SubMesh>						subMeshes;
	std::vector<std::shared_ptr<Node>>			nodes;
	std::vector<std::shared_ptr<IkSolver>>		ikSolvers;
	std::vector<std::unique_ptr<Morph>>			morphs;

	/// 애니메이션 평가에 필요한 기본 상태를 초기화한다.
	void InitializeAnimation();
	/// 현재 애니메이션 상태를 기준 애니메이션으로 저장한다.
	void SaveBaseAnimation() const;
	/// 저장된 기준 애니메이션 상태를 지운다.
	void ClearBaseAnimation() const;
	/// 프레임 애니메이션 평가를 시작하기 전 상태를 준비한다.
	void BeginAnimation();
	/// 현재 모프 가중치를 반영해 모프 애니메이션을 갱신한다.
	void UpdateMorphAnimation();
	/// 노드 애니메이션과 IK를 갱신한다.
	void UpdateNodeAnimation(bool afterPhysicsAnim) const;
	/// 물리 월드의 강체와 제약 상태를 초기 위치로 되돌린다.
	void ResetPhysics() const;
	/// 경과 시간만큼 물리 시뮬레이션을 진행하고 본에 반영한다.
	void UpdatePhysicsAnimation(float elapsed) const;
	/// 모델의 전체 변형 결과를 현재 상태 기준으로 갱신한다.
	void Update();
	/// 지정한 애니메이션 프레임과 물리 시간으로 모든 애니메이션 단계를 갱신한다.
	void UpdateAllAnimation(const Animation* anim, float frame, float physicsElapsed);
	/// PMX 모델과 관련 리소스를 파일에서 로드한다.
	bool Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir);
	/// 모델이 소유한 리소스와 런타임 상태를 해제한다.
	void Destroy();

private:
	std::string									m_modelName;
	std::string									m_englishModelName;
	std::string									m_comment;
	std::string									m_englishComment;
	std::vector<glm::vec3>						m_normals;
	std::vector<glm::vec2>						m_uvs;
	std::vector<Vertex>							m_vertexBoneInfos;
	std::vector<glm::mat4>						m_transforms;
	std::vector<std::vector<PositionMorph>>		m_positionMorphs;
	std::vector<std::vector<UvMorph>>			m_uvMorphs;
	std::vector<std::vector<MaterialMorph>>		m_materialMorphs;
	std::vector<std::vector<BoneMorph>>			m_boneMorphs;
	std::vector<std::vector<GroupMorph>>		m_groupMorphs;
	std::vector<glm::vec3>						m_morphPositions;
	std::vector<glm::vec4>						m_morphUVs;
	std::vector<Material>						m_initMaterials;
	std::vector<MaterialMorph>					m_mulMaterialFactors;
	std::vector<MaterialMorph>					m_addMaterialFactors;
	glm::vec3									m_bboxMin;
	glm::vec3									m_bboxMax;
	std::vector<std::reference_wrapper<Node>>	m_sortedNodes;
	std::unique_ptr<Physics>					m_physics;
	std::vector<std::unique_ptr<RigidBody>>		m_rigidBodies;
	std::vector<std::unique_ptr<Joint>>			m_joints;
	uint32_t									m_parallelUpdateCount = 0;
	std::vector<UpdateRange>					m_updateRanges;
	std::vector<std::future<void>>				m_parallelUpdateFutures;

	/// 병렬 버텍스 갱신에 사용할 작업 범위를 구성한다.
	void SetupParallelUpdate();
	/// 지정된 버텍스 범위의 스키닝 결과를 갱신한다.
	void Update(const UpdateRange& range);
	/// 단일 모프를 지정한 가중치로 평가한다.
	void EvalMorph(const Morph* morph, float weight);
	/// 위치 모프 데이터를 버텍스 위치에 적용한다.
	void MorphPosition(const std::vector<PositionMorph>& morphData, float weight);
	/// UV 모프 데이터를 버텍스 UV에 적용한다.
	void MorphUV(const std::vector<UvMorph>& morphData, float weight);
	/// 재질 모프 누적을 시작하기 위해 재질 계수를 초기화한다.
	void BeginMorphMaterial();
	/// 누적된 재질 모프 결과를 최종 재질에 반영한다.
	void EndMorphMaterial();
	/// 재질 모프 데이터를 현재 재질 계수에 누적한다.
	void MorphMaterial(const std::vector<MaterialMorph>& morphData, float weight);
	/// 본 모프 데이터를 노드 애니메이션 변환에 적용한다.
	void MorphBone(const std::vector<BoneMorph>& morphData, float weight) const;
};
