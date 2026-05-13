#pragma once

#include "Reader.h"

#include <map>
#include <ranges>

struct Morph;
class IkSolver;
class Node;
class Model;

struct Bezier {
	glm::vec2	p1;
	glm::vec2	p2;

	// VMD 보간 바이트 네 개를 정규화된 2D Bezier 제어점으로 변환한다.
	void Assign(int x0, int x1, int y0, int y1);
	// 주어진 x 시간에 대응하는 Bezier 보간 값을 계산한다.
	float Evaluate(float time) const;

private:
	// 3차 Bezier 곡선의 단일 축 값을 계산한다.
	static float EvaluateBezier(float t, float p1, float p2);
	// 주어진 x 시간에 대응하는 Bezier 매개변수 t를 이분 탐색으로 찾는다.
	static float FindBezierX(float time, float x1, float x2);
};

struct NodeAnimationKey {
	int32_t		time;
	glm::vec3	translate;
	glm::quat	rotate;
	Bezier		txBezier;
	Bezier		tyBezier;
	Bezier		tzBezier;
	Bezier		rotBezier;

	// VMD 모션 키에서 노드 애니메이션 키 값을 채운다.
	void ApplyMotion(const VmdReader::VmdMotion& motion);
};

struct MorphAnimationKey {
	int32_t	time;
	float	morphWeight;
};

struct IkAnimationKey {
	int32_t	time;
	bool	ikEnable;
};

struct NodeAnimationTrack {
	std::shared_ptr<Node> node;
	std::vector<NodeAnimationKey> keys;
};

struct IkAnimationTrack {
	std::shared_ptr<IkSolver> ikSolver;
	std::vector<IkAnimationKey> keys;
};

struct MorphAnimationTrack {
	std::shared_ptr<Morph> morph;
	std::vector<MorphAnimationKey> keys;
};

class AnimationHelper {
public:
	// 지정 시간보다 큰 첫 번째 키를 찾는다.
	template <typename Keys>
	static auto FindUpperKey(const Keys& keys, const float t) {
		return std::ranges::upper_bound(keys, t, std::less{}, [](const auto& key) {
			return static_cast<float>(key.time);
		});
	}
	// 트랙 맵의 바인딩된 트랙만 시간순으로 정렬해 트랙 목록으로 옮긴다.
	template <typename TrackMap, typename TrackList, typename TimeMember>
	static void FlushTrackMap(TrackMap& trackMap, TrackList& tracks, TimeMember timeMember) {
		tracks.clear();
		for (auto& track : trackMap | std::views::values) {
			if (!IsTrackBound(track))
				continue;
			std::ranges::sort(track.keys, {}, timeMember);
			tracks.emplace_back(std::move(track));
		}
	}
	// 노드 트랙이 실제 노드에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
	// IK 트랙이 실제 IK 솔버에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
	// 모프 트랙이 실제 모프에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }
	// 노드 트랙 목록을 노드 이름 기반 맵으로 옮긴다.
	static std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks);
	// IK 트랙 목록을 IK 노드 이름 기반 맵으로 옮긴다.
	static std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks);
	// 모프 트랙 목록을 모프 이름 기반 맵으로 옮긴다.
	static std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks);
};

class Animation {
	std::vector<NodeAnimationTrack> nodeTracks;
	std::vector<IkAnimationTrack> ikTracks;
	std::vector<MorphAnimationTrack> morphTracks;
	
	// 이름과 일치하는 모델 노드를 찾는다.
	std::shared_ptr<Node> FindNodeByName(const std::string& name) const;
	// 이름과 일치하는 IK 솔버를 찾는다.
	std::shared_ptr<IkSolver> FindIkSolverByName(const std::string& name) const;
	// 이름과 일치하는 모델 모프를 찾는다.
	std::shared_ptr<Morph> FindMorphByName(const std::string& name) const;
	// VMD 본 모션을 노드 애니메이션 트랙에 병합한다.
	void AddNodeAnimations(const VmdReader& vmd);
	// VMD IK 키를 IK 애니메이션 트랙에 병합한다.
	void AddIkAnimations(const VmdReader& vmd);
	// VMD 모프 키를 모프 애니메이션 트랙에 병합한다.
	void AddMorphAnimations(const VmdReader& vmd);
	// 지정 시간의 노드 애니메이션을 평가한다.
	void EvaluateNodes(float t, float animWeight) const;
	// 지정 시간의 IK 애니메이션을 평가한다.
	void EvaluateIks(float t, float animWeight) const;
	// 지정 시간의 모프 애니메이션을 평가한다.
	void EvaluateMorphs(float t, float animWeight) const;
	
public:
	std::shared_ptr<Model> model;

	// VMD 데이터를 모델 애니메이션 트랙에 추가한다.
	bool Add(const VmdReader& vmd);
	// 애니메이션 트랙과 연결 상태를 해제한다.
	void Destroy();
	// 지정한 시간의 애니메이션 값을 모델에 평가해 적용한다.
	void Evaluate(float t, float animWeight = 1.0f) const;
	// 물리 상태를 지정한 애니메이션 시간에 맞춰 동기화한다.
	void SyncPhysics(float t) const;
};

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
	Bezier		ixBezier;
	Bezier		iyBezier;
	Bezier		izBezier;
	Bezier		rotateBezier;
	Bezier		distanceBezier;
	Bezier		fovBezier;
};

class CameraAnimation {
	std::vector<CameraAnimationKey>	keys;
	
public:
	Camera camera;

	// VMD 카메라 키를 읽어 카메라 애니메이션을 생성한다.
	bool Create(const VmdReader& vmd);
	// 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
	void Evaluate(float t);
};
