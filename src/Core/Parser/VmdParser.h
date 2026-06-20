#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	enum class ShadowType : uint8_t {
		Off,
		Mode1,
		Mode2
	};

	class VmdParser {
	public:
		struct VmdHeader {
			char header[30];
			char modelName[20];
		};

		struct VmdMorph {
			char			blendShapeName[15]{};
			uint32_t		frame{};
			float			weight{};
		};

		struct VmdCamera {
			uint32_t		frame;
			float			distance;
			glm::vec3		interest;
			glm::vec3		rotate;
			uint8_t			interpolation[24];
			uint32_t		viewAngle;
			uint8_t			isPerspective;
		};

		struct VmdLight {
			uint32_t	frame;
			glm::vec3	color;
			glm::vec3	position;
		};

		struct VmdShadow {
			uint32_t	frame;
			ShadowType	shadowType;
			float		distance;
		};

		struct VmdIkInfo {
			char			name[20]{};
			uint8_t			enable{};
		};

		struct VmdIk {
			uint32_t	frame;
			uint8_t		show;
			std::vector<VmdIkInfo>	ikInfos;
		};

		struct VmdMotion {
			char			boneName[15];
			uint32_t		frame;
			glm::vec3		translate;
			glm::quat		quaternion;
			uint8_t			interpolation[64];
		};

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
		void ReadHeader(std::istream& is);
		// 본 모션 키프레임 목록을 읽는다.
		void ReadMotion(std::istream& is);
		// 모프 키프레임 목록을 읽는다.
		void ReadBlendShape(std::istream& is);
		// 카메라 키프레임 목록을 읽는다.
		void ReadCamera(std::istream& is);
		// 라이트 키프레임 목록을 읽는다.
		void ReadLight(std::istream& is);
		// 그림자 키프레임 목록을 읽는다.
		void ReadShadow(std::istream& is);
		// IK 표시/활성화 키프레임 목록을 읽는다.
		void ReadIk(std::istream& is);
		// 이전에 읽은 VMD 데이터를 초기화한다.
		void Clear();

	public:
		const VmdData& GetData() const { return data; }
		
		// VMD 파일 전체를 읽어 모션/카메라/모프 데이터를 저장한다.
		bool ReadFile(const std::filesystem::path& filename);
	};
}
