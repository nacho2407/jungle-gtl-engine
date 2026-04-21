# Render Register Convention

이 문서는 렌더 시스템 전체에서 사용하는 register slot 규약을 정리합니다.

---

## 1. 전역 규약

다음 slot들은 가능한 한 모든 pass / shader에서 동일한 의미를 유지합니다.

### ConstantBuffer

| Slot | 이름 | 의미 |
|---|---|---|
| b0 | FrameBuffer | frame / camera / view 공통 데이터 |
| b1 | PerObjectBuffer | object transform / object color |
| b4 | GlobalLightBuffer | ambient / directional / light count |

### Shader Resource

| Slot | 이름 | 의미 |
|---|---|---|
| t6 | LocalLights | StructuredBuffer<FLocalLightInfo> |
| t10 | SceneDepth | scene depth texture |
| t11 | SceneColor | scene color texture |
| t13 | StencilTex | stencil SRV |

### Sampler

| Slot | 이름 | 의미 |
|---|---|---|
| s0 | LinearClamp | clamp linear sampling |
| s1 | LinearWrap | wrap linear sampling |
| s2 | PointClamp | point clamp sampling |

---

## 2. 로컬 규약

다음 slot들은 전역 의미를 가지지 않습니다.

### ConstantBuffer

| Slot | 이름 | 의미 |
|---|---|---|
| b2 | PerShader0 | pass / shader 전용 constant buffer |
| b3 | PerShader1 | pass / shader 전용 constant buffer |

### Shader Resource

| Slot | 이름 | 의미 |
|---|---|---|
| t0 ~ t5 | pass-local SRV | pass / shader 전용 texture input |

### UAV

| Slot | 이름 | 의미 |
|---|---|---|
| u0 ~ | pass-local UAV | compute / special pass 전용 |

---

## 3. 가장 중요한 규칙

### 규칙 1
`b0`, `b1`, `b4`, `t6`, `t10`, `t11`, `t13`, `s0`, `s1`, `s2`는 전역 slot로 유지합니다.

### 규칙 2
`b2`, `b3`, `t0 ~ t5`는 전역 의미를 부여하지 않습니다.

### 규칙 3
각 shader 파일 상단에는 반드시 다음 주석을 적습니다.

예시:

```hlsl
// Register Convention
// b0  : FrameBuffer
// b1  : PerObjectBuffer
// b2  : PerShader0 (this shader: FogBuffer)
// t10 : SceneDepth
// t11 : SceneColor
// s0  : LinearClamp