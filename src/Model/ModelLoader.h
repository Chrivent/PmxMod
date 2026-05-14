#pragma once

#include "Model.h"

namespace Chrivent {
	class PmxReader;

	class ModelLoader {
		Model& model;

		// PMX 정점 정보를 모델의 기본 정점 버퍼로 변환한다.
		void LoadVertices(const PmxReader& pmx, const glm::vec3& invZ) const;
		// PMX 면 인덱스를 렌더링용 인덱스 버퍼로 변환한다.
		bool LoadFaces(const PmxReader& pmx) const;
		// PMX 재질과 텍스처 참조를 모델 재질 목록으로 변환한다.
		void LoadMaterials(
			const PmxReader& pmx,
			const std::filesystem::path& modelDir,
			const std::filesystem::path& dataDir) const;
		// PMX 본 정보를 노드 계층, 변형 순서, IK 솔버로 변환한다.
		void LoadNodes(const PmxReader& pmx, const glm::vec3& invZ) const;
		// PMX 모프 정보를 타입별 모프 데이터로 변환한다.
		void LoadMorphs(const PmxReader& pmx, const glm::vec3& invZ) const;
		// 순환 참조가 있는 그룹 모프를 끊어 무한 재귀를 막는다.
		void FixInfiniteGroupMorphs() const;
		// PMX 강체와 조인트를 물리 월드에 등록한다.
		void LoadPhysics(const PmxReader& pmx) const;

	public:
		explicit ModelLoader(Model& model) : model(model) {}

		// PMX 파일을 읽어 모델 데이터 전체를 초기화한다.
		bool Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) const;
	};
}
