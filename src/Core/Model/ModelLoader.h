#pragma once

#include "Core/Model/Model.h"
#include "Core/Parser/PmxParser.h"

#include <expected>
#include <string>

namespace Chrivent {
	struct JointDefinition;
	struct RigidBodyDefinition;

	enum class ModelLoadErrorCode {
		Parse,
		UnsupportedFeature
	};

	// 모델 구성 실패의 종류와 사용자에게 전달할 메시지를 보관한다.
	struct ModelLoadError {
		ModelLoadErrorCode code = ModelLoadErrorCode::Parse;
		std::string message;
	};

	// 파싱된 PMX 데이터를 런타임 모델 객체로 구성한다.
	class ModelLoader {
		Model& model;

		// 현재 런타임이 지원하지 않는 PMX 기능이 포함되어 있는지 검증한다.
		static std::expected<void, ModelLoadError> ValidateSupportedFeatures(const PmxParser::PmxData& pmxData);
		// PMX 강체 정보를 파서와 독립적인 런타임 강체 정의로 변환한다.
		static RigidBodyDefinition CreateRigidBodyDefinition(const PmxParser::PmxRigidbody& rigidBody);
		// PMX 조인트 정보를 파서와 독립적인 런타임 조인트 정의로 변환한다.
		static JointDefinition CreateJointDefinition(const PmxParser::PmxJoint& joint);
		// PMX 정점 정보를 모델의 기본 정점 버퍼로 변환한다.
		void LoadVertices(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const;
		// PMX 면 인덱스를 렌더링용 인덱스 버퍼로 변환한다.
		void LoadFaces(const PmxParser::PmxData& pmxData) const;
		// PMX 재질과 텍스처 참조를 모델 재질 목록으로 변환한다.
		void LoadMaterials(
			const PmxParser::PmxData& pmxData,
			const std::filesystem::path& modelDir,
			const std::filesystem::path& defaultToonTextureDir) const;
		// PMX 본과 표시 프레임 정보를 노드 계층, 변형 순서, IK 솔버로 변환한다.
		void LoadNodes(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const;
		// PMX 모프 정보를 타입별 모프 데이터로 변환한다.
		void LoadMorphs(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const;
		// 순환 참조가 있는 그룹 모프를 끊어 무한 재귀를 막는다.
		void FixInfiniteGroupMorphs() const;
		// PMX 강체와 조인트를 물리 월드에 등록한다.
		void LoadPhysics(const PmxParser::PmxData& pmxData) const;

	public:
		explicit ModelLoader(Model& model) : model(model) {}

		// PMX 모델과 내장 공용 툰 텍스처를 파일에서 로드한다.
		std::expected<void, ModelLoadError> Load(const std::filesystem::path& filepath,
			const std::filesystem::path& defaultToonTextureDir) const;
	};
}
