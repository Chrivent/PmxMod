#pragma once

#include <chrono>
#include <filesystem>
#include <memory>

#include <glm/glm.hpp>

namespace Chrivent {
	struct CameraAnimation;
	class InputManager;
	class Sound;
	class Viewer;

	struct Camera {
		glm::vec3 interest = glm::vec3(0, 10, 0);
		glm::vec3 rotate = glm::vec3(0, 0, 0);
		float distance = 50;
		float fov = glm::radians(30.0f);

		// ?꾩옱 移대찓???뚮씪誘명꽣濡?酉??됰젹??怨꾩궛?쒕떎.
		glm::mat4 CalcViewMatrix() const;
	};

	class CameraManager {
		bool paused = false;
		bool useMotionCamera = true;
		bool hasFreeCameraState = false;
		glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		float freeCamYaw = 0.0f;
		float freeCamPitch = 0.0f;
		std::unique_ptr<CameraAnimation> cameraAnim;

		// ?꾩옱 紐⑥뀡 移대찓???쒖젏?먯꽌 ?먯쑀 移대찓???꾩튂? ?뚯쟾媛믪쓣 ?숆린?뷀븳??
		void SyncFreeCameraToCurrentView(const Viewer& viewer);

	public:
		CameraManager();
		~CameraManager();

		// 移대찓?쇱? ?ъ깮 ?곹깭瑜?湲곕낯媛믪쑝濡?珥덇린?뷀븳??
		void Reset();
		// 移대찓??VMD ?뚯씪???쎌뼱 紐⑥뀡 移대찓???좊땲硫붿씠?섏쓣 以鍮꾪븳??
		void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
		// ?쇱떆?뺤?? ?ъ슫???숆린?붾? 諛섏쁺??酉곗뼱 ?좊땲硫붿씠???쒓컙??媛깆떊?쒕떎.
		void StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
		// ?낅젰 留ㅻ땲?媛 ?뺣━??議곗옉 ?곹깭瑜?移대찓?쇱? ?ъ깮 ?곹깭??諛섏쁺?쒕떎.
		void HandleInput(const InputManager& inputManager, const Viewer& viewer, Sound& music);
		// ?꾩옱 移대찓??紐⑤뱶??留욎떠 酉곗뼱??view/projection ?됰젹??媛깆떊?쒕떎.
		void UpdateCamera(Viewer& viewer) const;
	};
}
