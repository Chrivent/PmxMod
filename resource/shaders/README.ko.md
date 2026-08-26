# PmxMod 셰이더 패키지 계약

PmxMod는 `schemaVersion: 1` 포스트 프로세스 패키지를 지원합니다. 기존 입력 계약 안에서는 C++ 수정 없이 패키지 폴더의 HLSL과 JSON만으로 효과를 추가할 수 있습니다.

## 설치와 검색

패키지는 실행 파일 옆 `resource/shaders`의 바로 아래 폴더에 둡니다.

```text
resource/shaders/
└─ my-package/
   ├─ package.json
   ├─ include/
   │  └─ common.hlsli
   └─ effects/
      └─ my-effect/
         ├─ effect.json
         └─ effect.hlsl
```

PmxMod는 시작할 때 각 하위 폴더의 `package.json`을 읽습니다. 패키지를 추가하거나 파일을 바꾼 뒤에는 프로그램을 다시 실행해야 합니다. 패키지 ID는 설치된 패키지 전체에서 고유해야 합니다.

`resource/internal/shaders`는 모델, 엣지, 지면 그림자와 장면 입력용 엔진 셰이더 위치이며 패키지 검색 대상이 아닙니다.

## package.json

`package.json`은 패키지 정보와 포함할 효과 정의를 나열합니다.

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

- `schemaVersion`, `id`, `name`, `version`, `effects`는 필수입니다.
- `author`는 생략할 수 있습니다.
- `effects`에는 패키지 루트를 기준으로 한 상대 경로를 하나 이상 지정합니다.
- 효과 ID는 같은 패키지 안에서 고유해야 합니다.
- JSON, HLSL, include 경로는 패키지 폴더 밖을 가리킬 수 없습니다.

## effect.json

현재 지원하는 효과 형식은 `post_process`입니다.

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

- `inputs`는 빈 배열이어도 반드시 선언합니다. 사용할 수 있는 엔진 입력은 `scene_color`, `scene_depth`, `scene_velocity`입니다.
- `parameters`와 `resources`는 필요 없으면 생략할 수 있습니다.
- `passes`에는 하나 이상의 패스가 필요하며 이름이 중복될 수 없습니다.
- `vertexEntry`와 `pixelEntry`를 생략하면 각각 `VSMain`, `PSMain`을 사용합니다.
- 마지막 패스만 `effect_output`에 쓸 수 있으며 마지막 패스의 출력은 반드시 `effect_output`이어야 합니다.

## 입력과 출력

| 이름 | 내용 |
| --- | --- |
| `effect_input` | 앞 효과의 최종 색상. 첫 효과에서는 원본 장면 색상 |
| `scene_color` | 효과 순서와 무관한 원본 장면 색상 |
| `scene_depth` | 단일 샘플 장면 깊이. 불투명도 0.5 이상인 표면을 기록 |
| `scene_velocity` | 직전 프레임에서 현재 프레임까지의 화면 UV 이동량을 담은 `RG16_FLOAT` 텍스처 |
| `effect_output` | 현재 효과의 최종 색상 출력 |

`scene_color`, `scene_depth`, `scene_velocity`를 패스에서 읽을 때는 효과 최상위 `inputs`에도 같은 이름을 선언해야 합니다. `effect_input`과 효과 소유 리소스는 최상위 `inputs`에 넣지 않습니다.

`reads.slot`은 HLSL의 `Texture2D register(t0)`부터 `register(t7)`까지에 대응합니다. 같은 패스에서 슬롯을 중복할 수 없으며 HLSL이 실제로 선언한 텍스처 슬롯은 `reads`에도 있어야 합니다.

## HLSL 바인딩

모든 API가 같은 바인딩을 제공합니다.

| 레지스터 | 내용 |
| --- | --- |
| `b0` | 프레임 간격, viewport, 카메라와 history reset 상태 |
| `b1` | 효과별 스칼라 파라미터 64개 |
| `t0`~`t7` | 해당 패스의 `reads`에 연결된 텍스처 |
| `s0` | 선형 clamp sampler |

프레임 입력과 파라미터 배열의 정확한 HLSL 배치는 기본 패키지의 `pmxmod-motion-effects/include/post-process-frame.hlsli`와 `post-process-parameters.hlsli`를 참고할 수 있습니다. 다른 패키지의 파일을 직접 include할 수는 없으므로 필요한 선언은 자신의 패키지 안에 둡니다.

후처리 HLSL은 다음 규칙을 만족해야 합니다.

- HLSL Shader Model 6.0으로 SPIR-V 컴파일이 가능해야 합니다.
- include는 패키지 안의 리터럴 상대 경로만 사용합니다.
- constant buffer는 `b0`과 `b1`만 사용합니다.
- sampler는 `s0`만 사용합니다.
- storage image, storage buffer, subpass input, push constant는 사용할 수 없습니다.
- JSON의 `parameters`는 슬롯 0~63에서 ID와 슬롯이 중복되지 않아야 합니다.
- 각 파라미터의 `min`, `default`, `max`는 유한한 값이며 `min <= default <= max`여야 합니다.

## 중간 리소스와 history

다중 패스 효과는 `resources`를 선언합니다.

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

지원 형식은 `rgba8_unorm`, `rgba16_float`, `rgba32_float`입니다. 상대 해상도는 `full`, `half`, `quarter`, `eighth`이며 `size`로 고정 크기를 지정할 수도 있습니다. `resolution`과 `size`는 함께 쓸 수 없습니다.

- `transient`는 현재 프레임의 패스 사이에서만 유지됩니다. 먼저 출력한 뒤 읽어야 하며 같은 패스에서 동시에 읽고 쓸 수 없습니다.
- `history`는 프레임 사이에 유지되는 ping-pong 리소스입니다. 입력은 `focus.read`, 출력은 `focus.write`처럼 지정합니다.
- history write가 끝나면 같은 프레임의 다음 패스는 갱신된 read 값을 사용합니다.
- 사용자 리소스 이름에는 `.`을 쓸 수 없고 예약된 입력·출력 이름을 사용할 수 없습니다.
- 리소스 이름은 효과 내부 범위이므로 다른 효과와 같아도 충돌하지 않습니다.

활성 효과는 패널 목록 순서대로 연결됩니다. 중간 타깃 생성, 해상도 전환, history 상태와 API별 리소스 장벽은 공통 실행 계획이 처리합니다.

## C++ 계약을 확장해야 하는 경우

패스 수, 리소스 수, 해상도, 지원 형식, depth·velocity 사용 여부와 파라미터 수가 달라지는 것만으로는 C++ 수정이 필요하지 않습니다. 현재 목록에 없는 장면 데이터나 GPU 리소스 종류가 필요할 때만 모든 패키지가 공유할 새 엔진 입력과 다음 schema version을 추가합니다.
