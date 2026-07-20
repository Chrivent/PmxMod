#pragma once

#include "Core/Parser/BinaryReader.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	enum class ShadowType : uint8_t {
		Off,
		Mode1,
		Mode2
	};

	// VMD 바이너리를 본, 모프, 카메라, 조명과 IK 키로 해석한다.
	class VmdParser {
	public:
		// VMD 서명과 대상 모델 이름을 보관한다.
		struct VmdHeader {
			char header[30];
			char modelName[20];
		};

		// 한 프레임의 VMD 모프 가중치를 보관한다.
		struct VmdMorph {
			char			blendShapeName[15]{};
			uint32_t		frame{};
			float			weight{};
		};

		// 한 프레임의 VMD 카메라 값과 보간 데이터를 보관한다.
		struct VmdCamera {
			uint32_t		frame;
			float			distance;
			glm::vec3		interest;
			glm::vec3		rotate;
			uint8_t			interpolation[24];
			uint32_t		viewAngle;
			uint8_t			isPerspective;
		};

		// 한 프레임의 VMD 조명 색상과 위치를 보관한다.
		struct VmdLight {
			uint32_t	frame;
			glm::vec3	color;
			glm::vec3	position;
		};

		// 한 프레임의 VMD 그림자 모드와 거리를 보관한다.
		struct VmdShadow {
			uint32_t	frame;
			ShadowType	shadowType;
			float		distance;
		};

		// IK 이름과 활성 상태를 보관한다.
		struct VmdIkState {
			char			name[20]{};
			uint8_t			enable{};
		};

		// 한 프레임의 모델 표시 여부와 IK 상태 목록을 보관한다.
		struct VmdIk {
			uint32_t	frame;
			uint8_t		show;
			std::vector<VmdIkState>	ikStates;
		};

		// 한 프레임의 VMD 본 변환과 보간 데이터를 보관한다.
		struct VmdMotion {
			char			boneName[15];
			uint32_t		frame;
			glm::vec3		translate;
			glm::quat		quaternion;
			uint8_t			interpolation[64];
		};

		// 한 VMD 파일에서 읽은 모든 키프레임 섹션을 묶어 보관한다.
		struct VmdData {
			VmdHeader					header;
			std::vector<VmdMotion>		motions;
			std::vector<VmdMorph>		morphs;
			std::vector<VmdCamera>		cameras;
			std::vector<VmdLight>		lights;
			std::vector<VmdShadow>		shadows;
			std::vector<VmdIk>			iks;
		};

	private:
		VmdData data{};

		// VMD 헤더와 대상 모델 이름을 읽는다.
		void ReadHeader(BinaryReader& reader);
		// 본 모션 키프레임 목록을 읽는다.
		void ReadMotion(BinaryReader& reader);
		// 모프 키프레임 목록을 읽는다.
		void ReadBlendShape(BinaryReader& reader);
		// 카메라 키프레임 목록을 읽는다.
		void ReadCamera(BinaryReader& reader);
		// 라이트 키프레임 목록을 읽는다.
		void ReadLight(BinaryReader& reader);
		// 그림자 키프레임 목록을 읽는다.
		void ReadShadow(BinaryReader& reader);
		// IK 표시/활성화 키프레임 목록을 읽는다.
		void ReadIk(BinaryReader& reader);
		// 읽은 VMD 키프레임의 숫자와 보간 데이터를 검증한다.
		void ValidateData(BinaryReader& reader) const;
		// 이전에 읽은 VMD 데이터를 초기화한다.
		void Clear();

	public:
		const VmdData& GetData() const { return data; }
		
		// VMD 스트림 전체를 읽어 내부 데이터 구조에 저장한다.
		std::expected<void, ParseError> Read(std::istream& stream);
		// VMD 파일 전체를 읽어 모션/카메라/모프 데이터를 저장한다.
		std::expected<void, ParseError> ReadFile(const std::filesystem::path& filename);
	};
}
