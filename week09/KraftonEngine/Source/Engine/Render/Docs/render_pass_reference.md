# 06. Render Pass Reference

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-24 |
| 버전 | 1.0 |

> 사용 슬롯 열은 현재 코드에서 pass가 직접 바인딩하는 슬롯과 draw command 관례상 사용하는 대표 슬롯을 함께 정리한 것이다.  
> 일부 슬롯은 셰이더 variant 또는 draw command 구성 방식에 따라 달라질 수 있으므로, 최종 기준은 `RenderBindingSlots.h`, `BuildDrawCommand.cpp`, 각 pass 구현 파일을 따른다.

## 1. Scene Passes

### 1.1 `DepthPrePass`

| 항목 | 내용 |
|---|---|
| 역할 | scene depth를 먼저 기록하는 depth pre-pass |
| 소속 Pipeline | `Lit`, `Unlit`, `WorldNormal`, `SceneDepth` |
| 사용 슬롯 | `b0`, `b1` |
| 주요 입력 | scene primitive, draw command range(`ERenderPass::DepthPre`) |
| 주요 출력 | viewport depth/stencil |
| 비고 | color target 없이 depth만 바인딩한다. 시작 시 depth/stencil을 clear한다. PS SRV 0~5를 null로 비운다. |

### 1.2 `LightCullingPass`

| 항목 | 내용 |
|---|---|
| 역할 | tile-based local light culling compute 작업 수행 |
| 소속 Pipeline | `Lit` |
| 사용 슬롯 | `b0`, `t1`, `t6`, `u0`* |
| 주요 입력 | depth copy SRV, local light SRV, frame CB, `FTileBasedLightCulling` |
| 주요 출력 | per-tile light mask, debug hit map |
| 비고 | draw pass가 아니라 compute 성격의 pass다. `Wireframe`에서는 실제 dispatch 대신 debug hit map만 비운다. |

\* UAV 슬롯 번호는 실제 `TileBasedLightCulling` 구현 기준으로 표기하는 게 가장 정확하다. 문서에서는 compute 결과 UAV를 사용한다고 적고, 필요하면 구현 파일 기준으로 보강하는 게 좋다.

### 1.3 `OpaquePass`

| 항목 | 내용 |
|---|---|
| 역할 | opaque geometry를 렌더한다 |
| 소속 Pipeline | `Lit`, `Unlit`, `WorldNormal` |
| 사용 슬롯 | `b0`, `b1`, `b2`, `t0`, `t1`, `t2`, `t6`, `t7`, `t8` |
| 주요 입력 | visible primitive, material texture, per-object CB, local light mask SRV |
| 주요 출력 | viewport color 또는 `FViewModeSurfaces`의 base surface |
| 비고 | ViewMode 설정이 있으면 `FViewModeSurfaces`에 렌더하고, 아니면 viewport RTV에 직접 렌더한다. `t7`에 tile mask, `t8`에 debug hit map, `b2`에 light culling params를 사용한다. |

### 1.4 `DecalPass`

| 항목 | 내용 |
|---|---|
| 역할 | decal을 적용한다 |
| 소속 Pipeline | `Lit`, `Unlit`, `WorldNormal` |
| 사용 슬롯 | `t1`, `t2`, `t3`, `t10` |
| 주요 입력 | decal proxy, depth copy SRV, base/auxiliary surface SRV |
| 주요 출력 | viewport color 또는 `FViewModeSurfaces`의 modified surface |
| 비고 | `t10`에 depth copy를 바인딩한다. base surface 입력은 `t1~t3`에 들어간다. lit 계열에서는 modified surface를 갱신하고, unlit 계열에서는 viewport에 직접 반영한다. |

### 1.5 `LightingPass`

| 항목 | 내용 |
|---|---|
| 역할 | intermediate surface를 읽어 최종 조명 결과를 합성한다 |
| 소속 Pipeline | `Lit` |
| 사용 슬롯 | `b2`, `b4`, `t0`, `t1`, `t2`, `t3`, `t4`, `t5`, `t6`, `t7`, `t8`, `t10` |
| 주요 입력 | `FViewModeSurfaces`, depth copy SRV, global light CB, local light SRV, per-tile light mask |
| 주요 출력 | viewport color |
| 비고 | fullscreen pass다. `b4`에 global light, `b2`에 light culling params, `t0~t5`에 surface 입력, `t6`에 local lights, `t7`에 tile mask, `t8`에 debug hit map, `t10`에 depth copy를 사용한다. |

### 1.6 `NonLitViewModePass`

| 항목 | 내용 |
|---|---|
| 역할 | view mode 전용 시각화 결과를 fullscreen으로 출력한다 |
| 소속 Pipeline | `WorldNormal`, `SceneDepth` |
| 사용 슬롯 | `b2`, `t10`, `t1`* |
| 주요 입력 | scene depth SRV 또는 normal surface SRV, scene depth 시각화용 CB |
| 주요 출력 | viewport color |
| 비고 | `SceneDepth`에서는 `b2`에 `FSceneDepthPCBData`, `t10`에 depth copy를 사용한다. `WorldNormal`에서는 normal surface SRV를 사용한다. |

\* `WorldNormal` 입력은 draw command의 `DiffuseSRV`로 들어가므로 실제 슬롯은 fullscreen shader 바인딩 규약상 `t0` 또는 관례 슬롯으로 해석될 수 있다. 문서에는 “normal surface SRV”라고 병기하는 편이 안전하다.

## 2. PostProcess Passes

### 2.1 `HeightFogPass`

| 항목 | 내용 |
|---|---|
| 역할 | height fog를 fullscreen으로 적용한다 |
| 소속 Pipeline | `PostProcessPipeline` |
| 사용 슬롯 | `b2`, `t10`, `t11` |
| 주요 입력 | scene color copy SRV, depth copy SRV, fog constant buffer |
| 주요 출력 | viewport color |
| 비고 | `t11` scene color, `t10` depth copy, `b2` fog 상수를 사용한다. |

### 2.2 `FXAAPass`

| 항목 | 내용 |
|---|---|
| 역할 | FXAA 후처리를 적용한다 |
| 소속 Pipeline | `PostProcessPipeline` |
| 사용 슬롯 | `t11` |
| 주요 입력 | scene color copy SRV |
| 주요 출력 | viewport color |
| 비고 | fullscreen pass다. 주 입력은 scene color copy다. |

## 3. Overlay / Editor Passes

### 3.1 `LightHitMapPass`

| 항목 | 내용 |
|---|---|
| 역할 | light culling debug hit map을 화면에 overlay한다 |
| 소속 Pipeline | `OverlayPipeline` |
| 사용 슬롯 | `t8`, `t11` |
| 주요 입력 | scene color copy SRV, light hit map SRV |
| 주요 출력 | viewport color |
| 비고 | fullscreen overlay pass다. `t8`에 hit map SRV를 직접 바인딩한다. |

### 3.2 `DebugLinePass`

| 항목 | 내용 |
|---|---|
| 역할 | 디버그 라인을 렌더한다 |
| 소속 Pipeline | `OverlayPipeline` |
| 사용 슬롯 | `b0`, `b1` |
| 주요 입력 | line batch / debug line draw command |
| 주요 출력 | viewport color |
| 비고 | line draw command 제출 pass다. |

### 3.3 `SelectionMaskPass`

| 항목 | 내용 |
|---|---|
| 역할 | 선택된 오브젝트의 마스크를 stencil/depth에 기록한다 |
| 소속 Pipeline | `Outline` |
| 사용 슬롯 | `b0`, `b1` |
| 주요 입력 | selected primitive |
| 주요 출력 | viewport stencil/depth 기반 selection mask |
| 비고 | color target 없이 DSV만 바인딩한다. |

### 3.4 `OutlinePass`

| 항목 | 내용 |
|---|---|
| 역할 | selection mask를 바탕으로 outline 후처리를 적용한다 |
| 소속 Pipeline | `Outline` |
| 사용 슬롯 | `b2`, `t10`, `t11`, `t13` |
| 주요 입력 | scene color copy SRV, depth copy SRV, stencil copy SRV, outline CB |
| 주요 출력 | viewport color |
| 비고 | `t11` scene color, `t10` depth, `t13` stencil, `b2` outline 상수를 사용한다. |

### 3.5 `OverlayBillboardPass`

| 항목 | 내용 |
|---|---|
| 역할 | editor용 billboard를 overlay로 렌더한다 |
| 소속 Pipeline | `OverlayPipeline` |
| 사용 슬롯 | `b0`, `b1`, `t0` |
| 주요 입력 | overlay billboard draw command |
| 주요 출력 | viewport color |
| 비고 | billboard texture가 `t0`에 들어가는 구성이 일반적이다. |

### 3.6 `GizmoPass`

| 항목 | 내용 |
|---|---|
| 역할 | gizmo mesh를 렌더한다 |
| 소속 Pipeline | `OverlayPipeline` |
| 사용 슬롯 | `b0`, `b1`, `b2` |
| 주요 입력 | gizmo primitive |
| 주요 출력 | viewport color |
| 비고 | gizmo용 상수는 별도 per-shader CB를 쓸 수 있다. `GizmoOuter`, `GizmoInner` 두 범위를 순서대로 제출한다. |

### 3.7 `OverlayTextPass`

| 항목 | 내용 |
|---|---|
| 역할 | world text / overlay font text를 렌더한다 |
| 소속 Pipeline | `OverlayPipeline` |
| 사용 슬롯 | `b0`, `b1`, `t0`, `s0`* |
| 주요 입력 | overlay text draw command, font batch |
| 주요 출력 | viewport color |
| 비고 | 글리프 atlas texture를 읽는 구조다. sampler는 공용 sampler를 사용한다. |

\* 실제 sampler 선택은 셰이더와 draw command 구성에 따라 `s0/s1/s2` 중 하나를 쓸 수 있다. 보통 font atlas는 linear clamp 또는 point clamp 후보가 된다.

## 4. Present Pass

### 4.1 `PresentPass`

| 항목 | 내용 |
|---|---|
| 역할 | 최종 viewport 결과를 실제 back buffer로 복사한다 |
| 소속 Pipeline | `PresentPipeline` |
| 사용 슬롯 | 없음 |
| 주요 입력 | `ViewportRenderTexture` 또는 `SceneColorCopyTexture` |
| 주요 출력 | swapchain back buffer |
| 비고 | draw를 수행하지 않고 `CopyResource` 기반으로 present 직전 결과를 복사한다. |
