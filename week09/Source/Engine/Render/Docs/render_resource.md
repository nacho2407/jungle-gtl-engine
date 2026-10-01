# Render Resources

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-25 |
| 버전 | 1.1 |

## 1. 개요

> 렌더 리소스 구조: 실행 문맥(context), 런타임 리소스, constant buffer payload를 분리해 pass간 데이터 흐름을 명확하게 유지하자.

렌더 리소스 문서는 크게 다음 세 가지 축을 기준으로 정리한다.

1. `Context`
   - Pass와 Pipeline 실행 중 필요한 공용 상태와 리소스 참조를 전달하는 문맥이다.
2. `Runtime Resources`
   - 실제 렌더링 중 생성, 보관, 바인딩되는 GPU 리소스와 관리 객체다.
3. `Constant Buffer Payload`
   - CPU에서 구성해 GPU constant buffer로 업로드하는 데이터 레이아웃이다.

즉, `Context`는 실행에 필요한 참조를 연결하고, `Runtime Resources`는 실제 GPU 자원을 보관하며, `Constant Buffer Payload`는 shader가 읽을 데이터를 정의한다.

## 2. 리소스 분류

현재 렌더 리소스는 `Context`, `Runtime Resources`, `Constant Buffer Payload`의 세 관점으로 나누어 본다.

- `Context`
  - 실행 중 공용 상태와 리소스 참조를 전달하는 구조
- `Runtime Resources`
  - 실제 GPU resource, sampler, render target, intermediate surface, cache 관리 객체
- `Constant Buffer Payload`
  - CPU에서 구성해 constant buffer에 업로드하는 shader 입력 데이터

`Runtime Resources`는 다시 수명 기준으로 다음과 같이 나뉜다.

- `Frame lifetime`
  - 한 프레임 동안 공용으로 유지되는 리소스
- `Viewport lifetime`
  - viewport 크기 및 유효성에 종속되는 리소스
- `Persistent lifetime`
  - 여러 프레임에 걸쳐 재사용되는 리소스 또는 관리 객체

### 2.1 Context 계열

실행 중 공용 상태와 리소스 참조를 전달하는 문맥 구조들이다.
`Context` 계열은 리소스를 직접 소유하기보다, 현재 pass가 접근해야 할 상태와 runtime resource를 하나의 실행 범위로 묶어 전달한다.

| 타입 | Scope | 역할 |
|---|---|---|
| `FSceneView` | per-view | 카메라, 행렬, viewport, show flag, view mode 문맥 |
| `FRenderPipelineContext` | per-pipeline execution | 렌더 실행 전반에서 공통으로 참조되는 상위 실행 문맥 |
| `FViewportRenderTargets` | per-viewport | viewport 출력 타깃과 복사본 관리 |
| `FViewModeExecutionContext` | per-viewmode execution | 현재 ViewMode 실행 문맥 |
| `FRenderSubmissionContext` | per-submission | collect 결과 전달 문맥 |

### 2.2 `FRenderPipelineContext`

`FRenderPipelineContext`는 렌더 실행 전반에서 공통으로 참조되는 상위 실행 문맥이다.  
실행에 필요한 상태, runtime resource 참조, 하위 context를 묶어 각 Pass와 Pipeline에 전달한다.

현재 기준으로 다음 항목들을 포함한다.

- `SceneView`
- `Targets`
- `Scene`
- `Renderer`
- `Device`
- `Context`
- `Resources`
- `StateCache`
- `DrawCommandList`
- `RenderPassPresets`
- `Occlusion`
- `LightCulling`
- `LODContext`
- `ViewMode`
- `Submission`

즉, `FRenderPipelineContext`는 모든 runtime resource를 직접 소유하는 구조라기보다,  
실행에 필요한 context, runtime resource 참조, 하위 문맥을 묶는 상위 실행 문맥이다.

### 2.3 Frame-scoped Runtime Resources

한 프레임 동안 공용으로 유지되는 runtime GPU 리소스다.

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
| `TextBatch` | overlay text 렌더용 배치 |

### 2.4 Viewport-scoped Runtime Resources

viewport 크기 및 유효성에 종속되는 runtime GPU 리소스다.

| 타입 | 역할 |
|---|---|
| `ViewportRTV` | 현재 viewport 색상 출력 |
| `ViewportDSV` | 현재 viewport depth/stencil 출력 |
| `ViewportRenderTexture` | viewport color 대상 텍스처 |
| `SceneColorCopyTexture` | color 복사본 텍스처 |
| `SceneColorCopySRV` | color 복사본 SRV |
| `DepthCopySRV` | depth 복사본 SRV |
| `StencilCopySRV` | stencil 복사본 SRV |
| `ShadowMap Texture/SRV/DSV` | `ShadowMapPass`가 관리하는 shadow cube depth 리소스 |

### 2.5 ViewMode Intermediate Runtime Resources

`FViewModeSurfaces`는 `ScenePipeline` 내부의 ViewMode Pipeline이 사용하는 intermediate surface를 제공한다.  
이 리소스는 일반적으로 viewport 수명과 함께 관리된다.

| Slot | 의미 |
|---|---|
| `BaseColor` | 기본 색상 결과 |
| `Surface1` | shading model용 보조 surface |
| `Surface2` | shading model용 보조 surface |
| `ModifiedBaseColor` | decal 반영 후 색상 |
| `ModifiedSurface1` | decal 반영 후 보조 surface |
| `ModifiedSurface2` | decal 반영 후 보조 surface |

### 2.6 Persistent Runtime Resources

여러 프레임에 걸쳐 재사용되는 runtime resource 또는 관리 객체다.

| 타입 | 역할 |
|---|---|
| `PerObjectCBPool` | 재사용 가능한 constant buffer 풀 |
| shader cache | 셰이더 재사용 |
| state cache | 상태 객체 재사용 |
| registry | 실행 구조 및 선택 규칙 재사용 |

`ShadowMapPass`는 내부적으로 최대 5개의 shadow cube map 리소스를 유지한다. 각 shadow resource는 depth texture 1개, cube face별 DSV 6개, lighting 단계에서 읽기 위한 SRV 1개로 구성된다.

## 3. Constant Buffer Payload

CPU 측에서 구성한 뒤 GPU constant buffer로 업로드하는 데이터 구조다.
`Runtime Resources`가 constant buffer 객체를 보관한다면, `Constant Buffer Payload`는 그 버퍼에 들어갈 실제 데이터 레이아웃을 정의한다.

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
