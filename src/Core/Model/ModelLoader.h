#pragma once

#include "Core/Model/ModelTypes.h"
#include "Core/Parser/PmxParser.h"

#include <expected>
#include <filesystem>
#include <string>

namespace Chrivent {
	class Model;

	enum class ModelLoadErrorCode {
		Parse,
		UnsupportedFeature,
		Physics
	};

	// 모델 로드 실패 종류와 사용자에게 표시할 상세 내용을 전달한다.
	struct ModelLoadError {
		ModelLoadErrorCode code = ModelLoadErrorCode::Parse;
		std::string message;
	};

	// PMX 파일을 파싱하고 완성된 런타임 모델로 원자적으로 교체한다.
	class ModelLoader {
		// 현재 런타임이 지원하지 않는 PMX 기능이 포함되어 있는지 검증한다.
		static std::expected<void, ModelLoadError> ValidateSupportedFeatures(const PmxParser::PmxData& pmxData);
		// PMX 강체 정보를 파서와 독립적인 런타임 강체 정의로 변환한다.
		static RigidBodyDefinition CreateRigidBodyDefinition(const PmxParser::PmxRigidbody& rigidBody);
		// PMX 조인트 정보를 파서와 독립적인 런타임 조인트 정의로 변환한다.
		static JointDefinition CreateJointDefinition(const PmxParser::PmxJoint& joint);
		// PMX 스키닝 형식을 런타임 스키닝 형식으로 변환한다.
		static WeightType ConvertWeightType(PmxParser::WeightType weightType);
		// PMX 스피어 텍스처 모드를 런타임 모드로 변환한다.
		static SphereMode ConvertSphereMode(PmxParser::SphereMode sphereMode);
		// PMX 모프 형식을 런타임 모프 형식으로 변환한다.
		static MorphType ConvertMorphType(PmxParser::MorphType morphType);
		// PMX 재질 모프 연산을 런타임 연산으로 변환한다.
		static OpType ConvertOpType(PmxParser::OpType opType);
		// PMX 정점 정보를 모델의 기본 정점 버퍼로 변환한다.
		static void LoadVertices(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ);
		// PMX 면 인덱스를 렌더링용 인덱스 버퍼로 변환한다.
		static void LoadFaces(Model& model, const PmxParser::PmxData& pmxData);
		// PMX 재질과 텍스처 참조를 모델 재질 목록으로 변환한다.
		static void LoadMaterials(Model& model, const PmxParser::PmxData& pmxData,
			const std::filesystem::path& modelDir, const std::filesystem::path& defaultToonTextureDir);
		// PMX 본과 표시 프레임 정보를 노드 계층, 변형 순서, IK 솔버로 변환한다.
		static void LoadNodes(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ);
		// PMX 모프 정보를 타입별 모프 데이터로 변환한다.
		static void LoadMorphs(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ);
		// 순환 참조가 있는 그룹 모프를 끊어 무한 재귀를 막는다.
		static void FixInfiniteGroupMorphs(Model& model);
		// PMX 강체와 조인트를 물리 월드에 등록한다.
		static std::expected<void, ModelLoadError> LoadPhysics(Model& model, const PmxParser::PmxData& pmxData);
		// 검증된 PMX 데이터를 지정 모델의 런타임 표현으로 조립한다.
		static std::expected<void, ModelLoadError> Build(Model& model, const PmxParser::PmxData& pmxData,
			const std::filesystem::path& modelDir, const std::filesystem::path& defaultToonTextureDir);

	public:
		// PMX 모델과 내장 공용 툰 텍스처를 파일에서 로드한다.
		static std::expected<void, ModelLoadError> Load(Model& model, const std::filesystem::path& filepath,
			const std::filesystem::path& defaultToonTextureDir);
	};
}
