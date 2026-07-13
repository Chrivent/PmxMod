// 모든 렌더링 API가 b0로 전달하는 프레임별 포스트 프로세스 입력이다.
cbuffer PostProcessFrameData : register(b0) {
    float FrameDeltaTime;
    float CameraNearPlane;
    float CameraFarPlane;
    float CameraVerticalFovRadians;
    float2 ViewportSize;
    float2 InverseViewportSize;
}
