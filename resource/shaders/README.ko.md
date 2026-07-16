# PmxMod 셰이더 패키지 계약

`schemaVersion: 1`부터 포스트 프로세스의 실행 순서와 기본 파라미터는 C++ 분기 대신 `effect.json`으로 선언한다. 기존 엔진 입력과 지원 형식 안에서는 HLSL과 JSON만 추가해 새 효과를 만들 수 있다.

## 패키지 구조

```text
my-package/
├─ package.json
├─ include/
│  └─ common.hlsli
└─ effects/
   └─ my-effect/
      ├─ effect.json
      └─ effect.hlsl
```

`package.json`은 패키지가 제공하는 효과 정의를 나열한다.

```json
{
  "schemaVersion": 1,
  "id": "author.my-package",
  "name": "My Package",
  "version": "1.0.0",
  "author": "Author",
  "effects": [
    "effects/my-effect/effect.json"
  ]
}
```

`resource/shaders` 아래에는 설치 가능한 패키지만 둔다. 내장 모델, 엣지, 지면 그림자와 렌더러 구현에만 필요한 셰이더는 `resource/internal/shaders`에 단독 HLSL로 두며 패키지 검색 대상에서 제외한다.

## 포스트 프로세스 효과

```json
{
  "schemaVersion": 1,
  "id": "my-effect",
  "name": "My Effect",
  "type": "post_process",
  "inputs": ["scene_depth"],
  "parameters": [
    {
      "id": "strength",
      "name": "Strength",
      "slot": 0,
      "default": 0.5,
      "min": 0.0,
      "max": 1.0
    }
  ],
  "passes": [
    {
      "name": "main",
      "shader": "effects/my-effect/effect.hlsl",
      "vertexEntry": "VSMain",
      "pixelEntry": "PSMain",
      "reads": [
        { "slot": 0, "resource": "effect_input" },
        { "slot": 1, "resource": "scene_depth" }
      ],
      "output": "effect_output"
    }
  ]
}
```

엔진 입력과 예약 출력은 다음과 같다.

- `effect_input`: 앞 효과의 최종 색상이다. 첫 효과에서는 원본 장면 색상이다.
- `scene_color`: 효과 순서와 무관한 원본 장면 색상이다. 사용할 때는 최상위 `inputs`에도 선언한다.
- `scene_depth`: 단일 샘플 장면 깊이다. material 불투명도 0.5 이상인 표면을 기록하며 사용할 때는 최상위 `inputs`에도 선언한다.
- `scene_velocity`: 직전 프레임에서 현재 프레임까지 이동한 화면 UV 단위 속도다. material 불투명도 0.5 이상인 표면을 `RG16_FLOAT`로 제공하며 사용할 때는 최상위 `inputs`에도 선언한다.
- `effect_output`: 현재 효과의 최종 출력이다. 마지막 패스만 출력할 수 있다.
- `reads.slot`: HLSL의 `Texture2D register(t0)`부터 `register(t7)`까지에 대응한다.

모든 포스트 프로세스 HLSL은 다음 공통 바인딩을 사용한다.

- `b0`: 프레임·카메라 입력이다. `pmxmod-motion-effects/include/post-process-frame.hlsli`와 같은 배치다.
- `b1`: 효과 파라미터 입력이다. `parameters`의 `slot`을 최대 64개까지 선언할 수 있다.
- `s0`: 선형 clamp sampler다.

`pmxmod-motion-effects/include/post-process-parameters.hlsli`를 포함하면 `ReadEffectParameter(slot)`으로 b1 값을 읽을 수 있다. 한 효과에 선언된 기본값은 그 효과의 모든 패스에 동일하게 전달된다. `id`와 `slot`은 효과 안에서 중복될 수 없으며 `default`는 `min`과 `max` 사이의 유한한 숫자여야 한다.

## 중간 리소스와 다중 패스

```json
"resources": [
  {
    "name": "small_color",
    "lifetime": "transient",
    "format": "rgba16_float",
    "resolution": "eighth"
  },
  {
    "name": "focus",
    "lifetime": "history",
    "format": "rgba32_float",
    "size": { "width": 1, "height": 1 }
  }
]
```

지원 형식은 `rgba8_unorm`, `rgba16_float`, `rgba32_float`다. 상대 해상도는 `full`, `half`, `quarter`, `eighth`를 지원하며 `size`로 고정 크기를 선언할 수도 있다.

- `transient`: 현재 프레임의 패스 사이에서만 사용하는 임시 리소스다. 쓰기 전에 읽을 수 없다.
- `history`: 프레임 사이에 유지되는 ping-pong 리소스다. 입력은 `focus.read`, 출력은 `focus.write`처럼 쓴다.
- history 쓰기 패스가 끝나면 출력이 즉시 read 쪽이 되어 같은 프레임의 다음 패스가 갱신된 값을 읽는다.
- 한 패스에서 같은 transient 리소스를 동시에 읽고 쓸 수 없다.
- 리소스 이름은 효과 내부 범위이므로 다른 효과와 같아도 충돌하지 않는다.

선택한 효과는 목록 순서대로 연결되며 각 효과의 `effect_output`이 다음 효과의 `effect_input`이 된다. 중간 타깃 생성, 해상도 전환, history 교체와 API별 리소스 장벽은 공통 실행 계획이 처리한다.

## 계약의 경계

패스 수, 임시 타깃 수, history 수, 해상도, 형식, depth 또는 velocity 사용 여부와 파라미터 수가 달라도 C++ 수정은 필요 없다. 현재 계약에 없는 장면 정보가 필요할 때만 새 엔진 입력과 스키마 버전을 설계한다. 이는 특정 효과 전용 분기가 아니라 모든 패키지가 공유하는 엔진 기능 확장으로 처리한다.
