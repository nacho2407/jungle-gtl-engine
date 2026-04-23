# Render Resources

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-22 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.2 |

## 1. 개요

현재 렌더 리소스 구조의 핵심은 다음 세 가지다.

1. `FFrameResources`
2. `FViewportRenderTargets`
3. `FViewModeSurfaces`

이 세 구조는 각각 다른 범위와 책임을 가진다.

- `FFrameResources`
  - 프레임 전반에서 공용으로 사용하는 버퍼와 sampler를 관리
- `FViewportRenderTargets`
  - 현재 viewport의 출력 타깃과 후속 pass 입력용 복사본을 관리
- `FViewModeSurfaces`
  - Opaque / Decal / Lighting 경로에서 사용하는 intermediate surface를 관리

## 2. 리소스 분류

### 2.1 Context 계열

실행 중 리소스를 직접 소유하거나 참조를 제공하는 문맥 구조들이다.

| 타입 | 역할 |
|---|---|
| `FRenderPipelineContext` | 패스 실행에 필요한 공용 문맥 |
| `FSceneView` | 카메라, 행렬, viewport, show flag, view mode 문맥 |
| `FViewportRenderTargets` | viewport 출력 타깃과 복사본 관리 |
| `FViewModeExecutionContext` | 현재 view mode 실행 문맥 |
| `FRenderSubmissionContext` | collect 결과 전달 문맥 |

### 2.2 Frame-scoped Resources

프레임 전반에서 공유되는 GPU 리소스다.

| 타입 | 역할 |
|---|---|
| `FrameBuffer` | frame/view 공용 constant buffer |
| `PerObjectConstantBuffer` | 기본 per-object constant buffer |
| `GlobalLightBuffer` | 글로벌 조명 constant buffer |
| `LinearClampSampler` | 공용 sampler |
| `LinearWrapSampler` | 공용 sampler |
| `PointClampSampler` | 공용 sampler |
| `LocalLightBuffer` | local light structured buffer |
| `LocalLightSRV` | local light SRV |
| `PerObjectCBPool` | proxy별 per-object CB 캐시 |
| `TextBatch` | overlay text 렌더용 배치 |

### 2.3 Viewport-scoped Resources

현재 viewport의 결과와 fullscreen 후속 pass 입력으로 쓰는 자원이다.

| 타입 | 역할 |
|---|---|
| `ViewportRTV` | 현재 viewport 색상 출력 |
| `ViewportDSV` | 현재 viewport depth/stencil 출력 |
| `ViewportRenderTexture` | viewport color 대상 텍스처 |
| `SceneColorCopyTexture` | color 복사본 텍스처 |
| `SceneColorCopySRV` | color 복사본 SRV |
| `DepthCopySRV` | depth 복사본 SRV |
| `StencilCopySRV` | stencil 복사본 SRV |

이 복사본들은 후속 pass가 현재 렌더 타깃을 안전하게 읽기 위해 사용한다.

### 2.4 ViewMode Intermediate Resources

`FViewModeSurfaces`는 scene shading 경로의 intermediate surface를 제공한다.

| Slot | 의미 |
|---|---|
| `BaseColor` | 기본 색상 결과 |
| `Surface1` | shading model용 보조 surface |
| `Surface2` | shading model용 보조 surface |
| `ModifiedBaseColor` | decal 반영 후 색상 |
| `ModifiedSurface1` | decal 반영 후 보조 surface |
| `ModifiedSurface2` | decal 반영 후 보조 surface |

Opaque pass는 base 계열 target에 기록하고,  
Decal pass는 modified 계열 target을 갱신한다.  
이후 Lighting 또는 ViewMode 후처리 pass가 이를 읽는다.

## 3. Constant Buffer Payload

CPU 측에서 구성해 GPU constant buffer로 업로드하는 데이터 구조다.

| 타입 | 역할 |
|---|---|
| `FPerObjectCBData` | model / normal matrix / color |
| `FFrameCBData` | view / projection / camera / time / wireframe 정보 |
| `FSubUVRegionCBData` | subUV 영역 정보 |
| `FGizmoCBData` | gizmo 렌더 파라미터 |
| `FOutlinePostProcessCBData` | outline 후처리 파라미터 |
| `FSceneDepthPCBData` | SceneDepth 시각화 파라미터 |
| `FFogCBData` | height fog 파라미터 |
| `FFXAACBData` | FXAA 파라미터 |

## 4. 설정 및 고정 정의 데이터

| 타입 | 역할 |
|---|---|
| `FRenderPassPreset` | pass별 리소스/바인딩/드로우 preset |
| `FRenderPassDrawPreset` | depth/blend/rasterizer/topology preset |
| `FRenderPipelineDesc` | pipeline tree 정의 |
| `FViewModePassConfig` | view mode별 pass enable/disable와 shader 구성 |
| `FViewModePassDesc` | 특정 render pass에 대응하는 shader variant 정보 |

## 5. 리소스 수명

| 수명 | 설명 | 예시 |
|---|---|---|
| Frame lifetime | 한 프레임 동안 공용으로 유지 | `FFrameResources` 내부 버퍼 |
| Viewport lifetime | 뷰포트 크기 및 유효성에 종속 | `FViewportRenderTargets`, `FViewModeSurfaces` |
| Persistent lifetime | 여러 프레임에 걸쳐 재사용 | shader cache, registry, CB cache |

## 6. 접근 패턴

| 패턴 | 설명 |
|---|---|
| Read-only | 이전 pass 결과를 이후 pass가 읽기만 함 |
| Write-only | 현재 pass가 새 결과를 기록함 |
| Read-Modify-Write | 기존 결과를 읽고 수정 결과를 별도 target에 씀 |
| Producer / Consumer | 한 pass가 쓰고 다음 pass가 읽음 |

### 6.1 Depth 계열

```text
DepthPre
  -> viewport depth
  -> depth copy
  -> SceneDepth / Fog / Outline / Decal 입력
```

### 6.2 Surface 계열

```text
Opaque
  -> BaseColor / Surface1 / Surface2
Decal
  -> ModifiedBaseColor / ModifiedSurface1 / ModifiedSurface2
Lighting or ViewModePostProcess
  -> 위 surface들을 SRV로 읽음
```

### 6.3 Scene Color 계열

```text
Lighting / HeightFog / FXAA / Overlay
  -> viewport color
  -> scene color copy
  -> 후속 fullscreen pass 입력
```

## 7. 주요 pass의 리소스 사용 예

### 7.1 OpaquePass

- 입력
  - material texture SRV
  - local light SRV
  - per-object / material / extra constant buffer
- 출력
  - `FViewModeSurfaces`의 base target

### 7.2 DecalPass

- 입력
  - depth copy
  - 기존 base surface
- 출력
  - modified surface

### 7.3 LightingPass

- 입력
  - base surface SRV
  - global light CB
  - local light SRV
  - light culling 결과
- 출력
  - viewport RTV

### 7.4 NonLitViewModePass

- 입력
  - SceneDepth variant: depth copy
  - WorldNormal variant: surface SRV
- 출력
  - viewport RTV

### 7.5 Overlay / PostProcess Pass

- 입력
  - scene color copy
  - depth copy
  - stencil copy
  - 필요 시 debug SRV
- 출력
  - viewport RTV

## 8. 정리

현재 리소스 구조는 다음처럼 이해하면 된다.

- 프레임 공용 리소스는 `FFrameResources`
- viewport 출력과 복사본은 `FViewportRenderTargets`
- scene shading intermediate surface는 `FViewModeSurfaces`

즉, 렌더러는 이 세 리소스 계층을 조합해  
scene pass, post-process pass, overlay pass를 연결한다.
