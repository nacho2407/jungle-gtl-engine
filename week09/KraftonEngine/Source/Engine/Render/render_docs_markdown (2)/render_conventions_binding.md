# Render Conventions: Binding

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-24 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.0 |

## 1. 목적

이 문서는 Render 모듈의 HLSL/C++ 바인딩 규약을 정리한다.  
대상은 다음 세 가지다.

- 슬롯 규약
- binding scope
- source of truth

새 shader나 pass를 추가할 때는 이 문서를 기준으로 기존 바인딩 의미를 유지한다.

## 2. Slot 분류

| 구분 | 표기 | 의미 |
|---|---|---|
| Constant Buffer | `b#` | 상수 버퍼 슬롯 |
| Shader Resource View | `t#` | 읽기 전용 리소스 슬롯 |
| Sampler | `s#` | sampler 슬롯 |
| Unordered Access View | `u#` | 읽기/쓰기 가능한 리소스 슬롯 |

## 3. Constant Buffer Slot 규약

현재 C++ 기준 슬롯 정의는 `Render/Resources/Bindings/RenderBindingSlots.h`에 둔다.

| Slot | 이름 | 의미 |
|---|---|---|
| `b0` | `ECBSlot::Frame` | frame/view 공용 상수 |
| `b1` | `ECBSlot::PerObject` | object 단위 상수 |
| `b2` | `ECBSlot::PerShader0` | pass/material/shader 추가 상수 |
| `b3` | `ECBSlot::PerShader1` | pass/material/shader 추가 상수 |
| `b4` | `ECBSlot::Light` | 글로벌 조명 상수 |

### 3.1 사용 원칙

- `b0`
  - 프레임 전체 공통 데이터
  - view, projection, camera, time 등
- `b1`
  - 오브젝트별 데이터
  - transform, normal matrix, color 등
- `b2`, `b3`
  - pass-local 또는 material-local 추가 파라미터
- `b4`
  - 글로벌 조명 데이터

## 4. System SRV Slot 규약

| Slot | 이름 | 의미 |
|---|---|---|
| `t6` | `ESystemTexSlot::LocalLights` | local light buffer SRV |
| `t10` | `ESystemTexSlot::SceneDepth` | depth copy SRV |
| `t11` | `ESystemTexSlot::SceneColor` | scene color copy SRV |
| `t13` | `ESystemTexSlot::Stencil` | stencil copy SRV |

이 슬롯들은 여러 pass가 공통 의미로 참조하므로,  
의미를 자주 바꾸지 않는 것을 원칙으로 한다.

## 5. 일반 SRV 사용 규약

draw command 및 pass 구성 관례상 아래 슬롯들이 자주 사용된다.

| Slot | 의미 |
|---|---|
| `t0` | primary texture 또는 fullscreen input |
| `t1` | normal texture 또는 auxiliary input |
| `t2` | specular / material texture |
| `t3` | modified base color 등 추가 input |
| `t4` | modified surface1 |
| `t5` | modified surface2 |
| `t7` | per-tile light mask |
| `t8` | light hit map debug SRV |

이 슬롯은 pass 성격에 따라 달라질 수 있지만,  
가능하면 기존 관례를 유지해 shader와 C++ 코드를 맞춘다.

## 6. Sampler Slot 규약

| Slot | 이름 |
|---|---|
| `s0` | `ESamplerSlot::LinearClamp` |
| `s1` | `ESamplerSlot::LinearWrap` |
| `s2` | `ESamplerSlot::PointClamp` |

이 sampler들은 보통 `FFrameResources`가 생성하고 공용으로 제공한다.

## 7. Binding Scope 규약

### 7.1 Frame Scope

프레임 전체에서 공통으로 유지되는 바인딩이다.

예:

- frame CB
- global light CB
- 공용 sampler

### 7.2 Pass Scope

특정 pass 동안 공통으로 유지되는 바인딩이다.

예:

- fog pass 전용 CB
- outline pass 전용 SRV
- fullscreen pass 입력

### 7.3 Draw Scope

개별 draw call마다 바뀌는 바인딩이다.

예:

- per-object CB
- material texture
- material parameter CB

## 8. Binding 분리 원칙

| 구분 | 설명 |
|---|---|
| Global binding | 여러 pass가 공통으로 참조 |
| Pass-local binding | 특정 pass 안에서만 유효 |
| Material binding | 머티리얼 단위로 변경 |
| Per-object binding | 오브젝트마다 변경 |

핵심은 **변경 빈도와 책임 범위가 다른 바인딩을 섞지 않는 것**이다.

## 9. Source of Truth

바인딩 규약의 실제 기준 파일은 다음과 같다.

| 파일 | 역할 |
|---|---|
| `Render/Resources/Bindings/RenderBindingSlots.h` | C++ 측 슬롯 상수 정의 |
| `Render/Resources/FrameResources.h` | frame 공용 CB, sampler, light buffer 정의 |
| `Render/Resources/Buffers/ConstantBufferData.h` | CB payload 구조 정의 |
| `Submission/Command/BuildDrawCommand.cpp` | draw command 수준 실제 바인딩 구성 |
| 각 pass 구현 파일 | pass-local SRV/CB 바인딩 및 unbind 처리 |

문서 규약보다 실제 구현이 우선되며,  
문서는 반드시 이 source of truth와 맞춰 유지해야 한다.

## 10. Pass 추가 시 체크 항목

새 pass나 shader를 추가할 때는 아래를 확인한다.

1. 어떤 CB를 `b0~b4` 중 어디에 둘지 결정한다.
2. 어떤 SRV를 system slot에 둘지, local slot에 둘지 결정한다.
3. sampler를 새로 둘 필요가 있는지 확인한다.
4. pass 종료 후 unbind가 필요한지 확인한다.
5. 후속 pass 입력과 충돌하지 않는지 확인한다.
6. `RenderPassPreset`과 실제 셰이더 바인딩이 일치하는지 확인한다.

## 11. 문서화 규칙

- 슬롯 번호와 의미를 함께 적는다.
- C++ enum 이름과 HLSL 슬롯 번호를 같이 표기한다.
- “어디서 정의되는지”와 “어디서 실제로 바인딩되는지”를 함께 적는다.
- 로컬 관례가 생기면 pass 문서에도 같이 남긴다.

## 12. 정리

Render 바인딩 규약의 핵심은 다음과 같다.

1. 공용 슬롯 의미를 쉽게 바꾸지 않는다.
2. frame / pass / draw scope를 구분한다.
3. 실제 기준은 `RenderBindingSlots.h`와 draw command 구성 코드에 둔다.
