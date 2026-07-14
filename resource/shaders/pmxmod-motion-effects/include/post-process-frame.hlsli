#ifndef PMXMOD_POST_PROCESS_FRAME_HLSLI
#define PMXMOD_POST_PROCESS_FRAME_HLSLI

// 모든 렌더링 API가 b0로 전달하는 프레임별 포스트 프로세스 입력이다.
cbuffer PostProcessFrameData : register(b0) {
    float FrameDeltaTime;
    float CameraNearPlane;
    float CameraFarPlane;
    float CameraVerticalFovRadians;
    float2 ViewportSize;
    float2 InverseViewportSize;
    float FrameHistoryReset;
    float FrameDataPadding0;
    float FrameDataPadding1;
    float FrameDataPadding2;
    float4 CameraWorldPosition;
    float4 PreviousCameraWorldPosition;
    float4 CameraWorldDirection;
    float4 PreviousCameraWorldDirection;
    float4 CameraWorldRight;
    float4 CameraWorldUp;
}

#endif
