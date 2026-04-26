# Render Pass Reference

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-25 |
| 버전 | 1.2 |

> 사용 슬롯 열은 현재 코드에서 pass가 직접 바인딩하는 슬롯과 draw command 관례상 사용하는 대표 슬롯을 함께 정리한 것이다.  
> 일부 슬롯은 셰이더 variant 또는 draw command 구성 방식에 따라 달라질 수 있으므로, 최종 기준은 `RenderBindingSlots.h`, `BuildDrawCommand.cpp`, 각 pass 구현 파일을 따른다.

## 1. Scene Passes

### 1.1 `DepthPrePass`

| 항목 | 내용 |
|---|---|
| 역할 | scene depth를 먼저 기록하는 depth pre-pass |
| 소속 파이프라인 | `DeferredLitPipeline`, `DeferredUnlitPipeline`, `DeferredWorldNormalPipeline`, `DeferredSceneDepthPipeline`, `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline`, `ForwardSceneDepthPipeline` |
| 주요 입력 | visible primitive, per-object constant buffer |
| 주요 출력 | viewport depth/stencil |

### 1.2 `ShadowMapPass`

| 항목 | 내용 |
|---|---|
| 역할 | shadow casting light 기준으로 shadow depth map을 생성한다 |
| 소속 파이프라인 | `DeferredLitPipeline`, `ForwardLitPipeline` |
| 주요 입력 | `VisibleLightProxies`, light별 `VisibleShadowCasters`, per-object constant buffer |
| 주요 출력 | shadow cube depth texture SRV/DSV |
| 비고 | lit 경로에서 공통으로 사용되며, 최대 5개의 shadow-casting light를 처리한다. point light는 cube map 6면을 각각 렌더한다. |

### 1.3 `LightCullingPass`

| 항목 | 내용 |
|---|---|
| 역할 | tile-based local light culling compute 작업 수행 |
| 소속 파이프라인 | `DeferredLitPipeline` |
| 주요 입력 | depth copy SRV, local light SRV, frame constant buffer |
| 주요 출력 | per-tile light mask, debug hit map |

### 1.4 `ForwardOpaquePass`

| 항목 | 내용 |
|---|---|
| 역할 | forward 경로에서 opaque geometry를 처리한다 |
| 소속 파이프라인 | `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline` |
| 주요 입력 | visible primitive, material texture, per-object CB, global light CB, local light SRV, shadow map SRV |
| 주요 출력 | viewport color |
| 비고 | view mode shader variant를 통해 lit/unlit/world normal permutation을 선택한다. lit forward 경로에서는 shadow map SRV도 함께 바인딩한다. |

### 1.5 `DeferredOpaquePass`

| 항목 | 내용 |
|---|---|
| 역할 | deferred 경로에서 opaque geometry를 렌더하고 GBuffer 성격의 intermediate surface를 기록한다 |
| 소속 파이프라인 | `DeferredLitPipeline`, `DeferredUnlitPipeline`, `DeferredWorldNormalPipeline` |
| 주요 입력 | visible primitive, material texture, per-object CB |
| 주요 출력 | `FViewModeSurfaces`의 base surface |

### 1.6 `DeferredDecalPass`

| 항목 | 내용 |
|---|---|
| 역할 | deferred 경로에서 decal을 intermediate surface에 적용한다 |
| 소속 파이프라인 | `DeferredLitPipeline`, `DeferredUnlitPipeline`, `DeferredWorldNormalPipeline` |
| 주요 입력 | decal proxy, depth copy SRV, base/auxiliary surface SRV |
| 주요 출력 | `FViewModeSurfaces`의 modified surface |

### 1.7 `DeferredLightingPass`

| 항목 | 내용 |
|---|---|
| 역할 | deferred intermediate surface를 읽어 최종 조명 결과를 합성한다 |
| 소속 파이프라인 | `DeferredLitPipeline` |
| 주요 입력 | `FViewModeSurfaces`, depth copy SRV, global light CB, local light SRV, per-tile light mask, shadow map SRV |
| 주요 출력 | viewport color |
| 비고 | deferred 전용 fullscreen lighting pass다. shadow map 결과를 읽어 조명 합성에 반영한다. |

### 1.8 `NonLitViewModePass`

| 항목 | 내용 |
|---|---|
| 역할 | view mode 전용 시각화 결과를 fullscreen으로 출력한다 |
| 소속 파이프라인 | `DeferredWorldNormalPipeline`, `DeferredSceneDepthPipeline`, `ForwardSceneDepthPipeline` |
| 주요 입력 | scene depth SRV 또는 normal surface SRV |
| 주요 출력 | viewport color |

## 2. Shared PostProcess Passes

### 2.1 `HeightFogPass`

| 항목 | 내용 |
|---|---|
| 역할 | height fog를 fullscreen으로 적용한다 |
| 소속 파이프라인 | `PostProcessPipeline` |
| 주요 입력 | scene color copy SRV, depth copy SRV, fog constant buffer |
| 주요 출력 | viewport color |

### 2.2 `FXAAPass`

| 항목 | 내용 |
|---|---|
| 역할 | FXAA 후처리를 적용한다 |
| 소속 파이프라인 | `PostProcessPipeline` |
| 주요 입력 | scene color copy SRV |
| 주요 출력 | viewport color |

## 3. Overlay / Editor Passes

### 3.1 `LightHitMapPass`

| 항목 | 내용 |
|---|---|
| 역할 | light culling debug hit map을 화면에 overlay한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### 3.2 `DebugLinePass`

| 항목 | 내용 |
|---|---|
| 역할 | 디버그 라인을 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### 3.3 `SelectionMaskPass`

| 항목 | 내용 |
|---|---|
| 역할 | 선택된 오브젝트의 마스크를 stencil/depth에 기록한다 |
| 소속 파이프라인 | `OutlinePipeline` |

### 3.4 `OutlinePass`

| 항목 | 내용 |
|---|---|
| 역할 | selection mask를 이용해 outline 후처리를 적용한다 |
| 소속 파이프라인 | `OutlinePipeline` |

### 3.5 `OverlayBillboardPass`

| 항목 | 내용 |
|---|---|
| 역할 | overlay billboard를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### 3.6 `GizmoPass`

| 항목 | 내용 |
|---|---|
| 역할 | transform gizmo를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### 3.7 `OverlayTextPass`

| 항목 | 내용 |
|---|---|
| 역할 | 텍스트 overlay를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

## 4. Forward Pipeline 메모

현재 `ForwardPipeline` 계열은 아래 네 하위 파이프라인을 가진다.
- `ForwardLitPipeline`
- `ForwardUnlitPipeline`
- `ForwardWorldNormalPipeline`
- `ForwardSceneDepthPipeline`

`ForwardOpaquePass`는 lit/unlit/world normal geometry 경로에서 공통으로 사용된다.
`ForwardWorldNormalPipeline`은 `ForwardOpaquePass`의 world normal shader variant가 viewport color에 직접 결과를 쓰는 direct output 경로를 사용한다.
이렇게 하면 intermediate texture write/read를 위한 추가 bandwidth 비용을 피할 수 있다.
