#ifndef PMXMOD_POST_PROCESS_PARAMETERS_HLSLI
#define PMXMOD_POST_PROCESS_PARAMETERS_HLSLI

// 모든 렌더링 API가 b1로 전달하는 효과별 스칼라 파라미터 입력이다.
cbuffer PostProcessParameterData : register(b1) {
    float4 EffectParameterValues[16];
}

// JSON parameter slot에 대응하는 스칼라 값을 반환한다.
float ReadEffectParameter(uint slot) {
    return EffectParameterValues[slot / 4][slot % 4];
}

#endif
