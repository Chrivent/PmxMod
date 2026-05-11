#pragma once

#include "Animation.h"
#include "../Reader.h"

struct Camera {
	glm::vec3	interest = glm::vec3(0, 10, 0);
	glm::vec3	rotate = glm::vec3(0, 0, 0);
	float		distance = 50;
	float		fov = glm::radians(30.0f);

	// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
	glm::mat4 CalcViewMatrix() const;
};

struct CameraAnimationKey {
	int32_t		time;
	glm::vec3	interest;
	glm::vec3	rotate;
	float		distance;
	float		fov;
	std::pair<glm::vec2, glm::vec2>	ixBezier;
	std::pair<glm::vec2, glm::vec2>	iyBezier;
	std::pair<glm::vec2, glm::vec2>	izBezier;
	std::pair<glm::vec2, glm::vec2>	rotateBezier;
	std::pair<glm::vec2, glm::vec2>	distanceBezier;
	std::pair<glm::vec2, glm::vec2>	fovBezier;
};

class CameraAnimation : public Animation {
public:
	Camera camera;

	// VMD 카메라 키를 읽어 카메라 애니메이션을 생성한다.
	bool Create(const VmdReader& vmd);
	// 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
	void Evaluate(float t);

private:
	std::vector<CameraAnimationKey>	keys;
};
