# PmxMod 셰이더 패키지 계약

`schemaVersion: 2`부터 포스트 프로세스는 C++ 코드가 아니라 `effect.json`이 실행 순서를 결정한다. 기존 엔진 입력과 지원 형식 안에서 효과를 추가할 때는 HLSL과 JSON만 추가하면 된다.

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

`package.json`은 효과 정의 파일들을 나열한다.

```json
{
  "schemaVersion": 2,
  "id": "author.my-package",
  "name": "My Package",
  "version": "1.0.0",
  "author": "Author",
  "effects": [
    "effects/my-effect/effect.json"
  ]
}
```

## 단일 패스 예제

```json
{
  "schemaVersion": 2,
  "id": "my-effect",
  "name": "My Effect",
  "type": "post_process",
  "inputs": ["scene_depth"],
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

- `effect_input`: 앞 효과의 최종 색상이다. 첫 효과에서는 원본 장면 색상이다.
- `scene_color`: 효과 순서와 무관한 원본 장면 색상이다. 사용할 때는 최상위 `inputs`에도 선언한다.
- `scene_depth`: 단일 샘플 장면 깊이다. 사용할 때는 최상위 `inputs`에도 선언한다.
- `effect_output`: 현재 효과의 최종 출력이다. 마지막 패스만 출력할 수 있다.
- `slot`: HLSL의 `Texture2D register(t0)`부터 `register(t7)`까지에 대응한다.

모든 포스트 프로세스 HLSL은 공통 프레임 데이터를 `b0`, 선형 clamp sampler를 `s0`으로 사용한다. 공통 프레임 데이터 레이아웃은 `pmxmod-motion-effects/include/post-process-frame.hlsli`를 참고한다.

## 중간 리소스와 다중 패스

```json
"resources": [
  {
    "name": "half_color",
    "lifetime": "transient",
    "format": "rgba16_float",
    "resolution": "half"
  },
  {
    "name": "focus",
    "lifetime": "history",
    "format": "rgba32_float",
    "size": { "width": 1, "height": 1 }
  }
]
```

지원 형식은 `rgba8_unorm`, `rgba16_float`, `rgba32_float`다. 상대 해상도는 `full`, `half`, `quarter`이고, `size`로 고정 크기를 선언할 수도 있다.

- `transient`: 현재 프레임의 패스 사이에서 사용하는 임시 리소스다. 쓰기 전에 읽을 수 없다.
- `history`: 프레임 사이에 유지되는 ping-pong 리소스다. 입력은 `focus.read`, 출력은 `focus.write`처럼 적는다.
- history 쓰기 패스가 끝나면 새 출력이 즉시 read 쪽이 되므로 같은 프레임의 다음 패스가 갱신된 값을 읽는다.
- 한 패스에서 같은 transient 리소스를 동시에 읽고 쓸 수 없다.
- 리소스 이름은 효과 내부에만 존재하므로 다른 효과와 이름이 같아도 충돌하지 않는다.

선택된 효과들은 목록 순서대로 연결되며, 각 효과의 `effect_output`이 다음 효과의 `effect_input`이 된다. 중간 렌더 타깃, 해상도 전환, history 초기화와 API별 barrier는 프로그램이 공통 실행 계획에서 자동으로 처리한다.

## 계약의 경계

패스 수, 임시 타깃 수, history 수, 해상도, 형식, depth 사용 여부가 달라져도 C++ 수정은 필요 없다. 반면 현재 계약에 없는 장면 정보(예: 물체별 모션 벡터나 법선 버퍼)를 새로 요구하면 먼저 엔진 입력으로 추가하고 `schemaVersion`을 올려야 한다. 이는 개별 효과 전용 분기가 아니라 모든 패키지가 공유하는 엔진 기능 확장으로 처리한다.
