# Render Execution Structure

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-25 |
| 버전 | 1.1 |

## 1. 개요

> 조립형 렌더 파이프라인: 원하는 렌더 실행단위를 자유롭게 조합해 파이프라인을 만들자.

렌더 실행 구조는 `Pass`와 `Pipeline`을 조합해 구성한다.

`Pass`는 실제 GPU 작업을 수행하는 leaf node이고, `Pipeline`은 pass와 하위 pipeline을 순서대로 묶는 실행 단위이다. 
즉, 하위 실행 단위를 조합해 상위 `Pipeline`을 만들고, 최상위 `Pipeline`이 전체 렌더 실행의 진입점이 된다. 이 구조는 general tree 형태로 볼 수 있다.

## 2. 실행 단위: "Render Node"

실행 흐름은 큰 단일 함수가 아니라, 실행 단위인 "렌더 노드(Render Node)"를 조합하여 구성한다.


- Pass는 가장 작은 실행 단위다.
- Pipeline은 Pass와 하위 Pipeline을 순서대로 묶어 실행하는 조합 단위다.
- `Pass`와 `Pipeline`은 모두 렌더 노드로 취급한다.
  - `Pass`: 실제 GPU 작업을 수행하는 말단 노드
  - `Pipeline`: 하위 렌더 노드 목록을 순서대로 실행하는 조합 노드

이러한 설계로 하여금 프로그래머가 원하는 실행 단위를 엮어 새로운 실행 단위를 조합해내거나 내부의 일부를 교체하는 등의 '렌더 실행단위 생성 · 편집'이 용이하다.

| 구분 | 역할 |
|---|---|
| Pass | 실제 draw, dispatch, fullscreen 작업 수행 |
| Pipeline | Pass/Pipeline을 묶어 실행 순서와 구조를 표현 |

## 3. 실행 흐름

전체적인 실행 흐름은 다음과 같다.

1. `Scene` / `Submission`에서 렌더 대상을 준비한다.
2. `Registry`는 pass, pipeline, view mode 매핑 규칙을 관리한다.
3. `Pipeline`은 미리 정의된 실행 트리 구조를 가진다.
4. `Runner`는 현재 `Context`에 맞는 pipeline을 순회한다.
5. leaf node인 `Pass`가 실제 GPU 작업을 수행한다.
6. 실행 상태와 리소스는 `Context`가 제공한다.

즉, 렌더 대상 준비와 실행 구조는 분리되어 있으며, `Execute` 계층은 미리 정의된 실행 트리를 선택하고 순회하는 역할을 담당한다.

## 4. 실행 트리: 최상위 Pipeline

`Root Pipeline`은 최상위 실행 진입점이며, 내부에 목적별 `Pipeline`과 `Pass`를 포함한다.

현재 실행 트리는 `Default`와 `Editor` 두 종류로 나뉜다.

### 4.1 DefaultRootPipeline

```text
DefaultRootPipeline
└─ ScenePipeline
   ├─ DeferredPipeline
   │  ├─ DeferredLitPipeline
   │  ├─ DeferredUnlitPipeline
   │  ├─ DeferredWorldNormalPipeline
   │  └─ DeferredSceneDepthPipeline
   ├─ ForwardPipeline
   │  ├─ ForwardLitPipeline
   │  ├─ ForwardUnlitPipeline
   │  ├─ ForwardWorldNormalPipeline
   │  └─ ForwardSceneDepthPipeline
   └─ PostProcessPipeline
```

### 4.2 EditorRootPipeline

```text
EditorRootPipeline
├─ ScenePipeline
│  ├─ DeferredPipeline
│  │  ├─ DeferredLitPipeline
│  │  ├─ DeferredUnlitPipeline
│  │  ├─ DeferredWorldNormalPipeline
│  │  └─ DeferredSceneDepthPipeline
│  ├─ ForwardPipeline
│  │  ├─ ForwardLitPipeline
│  │  ├─ ForwardUnlitPipeline
│  │  ├─ ForwardWorldNormalPipeline
│  │  └─ ForwardSceneDepthPipeline
│  └─ PostProcessPipeline
└─ OverlayPipeline
   ├─ LightHitMapPass
   ├─ DebugLinePass
   ├─ OutlinePipeline
   ├─ OverlayBillboardPass
   ├─ GizmoPass
   └─ OverlayTextPass
```

## 5. 실행 선택 규칙

### 5.1 Render Path 선택

에디터 설정의 `RenderShadingPath`에 따라 `ScenePipeline` 내부에서 아래 둘 중 하나만 활성화된다.
- `DeferredPipeline`
- `ForwardPipeline`

### 5.2 ViewMode 선택

선택된 렌더 경로 내부에서는 현재 `ViewMode`에 맞는 하위 파이프라인 하나만 활성화된다.  
활성화 규칙은 아래와 같다. 접두어 `*_`는 Forward 혹은 Deferred를 의미한다.

- `*_LitPipeline`: `Lit_Gouraud`, `Lit_Lambert`, `Lit_Phong`
- `*_UnlitPipeline`: `Unlit`, `Wireframe`
- `*_WorldNormalPipeline`: `WorldNormal`
- `*_SceneDepthPipeline`: `SceneDepth`

**ViewMode별 실행 흐름**

현재 ViewMdoe 선택 별 실행 흐름은 다음과 같다.

```text
1. DeferredLitPipeline        : DepthPre -> ShadowMap -> LightCulling -> Opaque -> Decal -> Lighting

2. ForwardLitPipeline         : DepthPre -> ShadowMap -> Opaque

3. *_UnlitPipeline            : DepthPre -> Opaque -> Decal

4. DeferredWorldNormalPipeline: DepthPre -> Opaque -> Decal -> NonLitView

5. ForwardWorldNormalPipeline : DepthPre -> Opaque

6. *_SceneDepthPipeline       : DepthPre -> NonLitView
```

`ShadowMapPass`는 현재 코드 기준으로 lit pipeline 공통 pass이며, shadow casting light를 순회하면서 shadow depth map을 먼저 만든다. deferred 경로에서는 이후 `DeferredLightingPass`가 이를 참조하고, forward lit 경로에서는 `ForwardOpaquePass` 입력으로 바인딩된다.


## 6. 디렉토리 구조

```text
Render/
├─ Execute/
│  ├─ Context/
│  ├─ Passes/
│  ├─ Registry/
│  └─ Runner/
├─ Scene/
├─ Submission/
├─ Resources/
└─ Visibility/
```

- `Execute/`
  - 렌더 실행 구조를 담당하는 핵심 계층이다.

- `Scene/`
  - 렌더 대상이 되는 scene 데이터와 proxy를 두는 계층이다.

- `Submission/`
  - scene에서 수집한 데이터를 실제 렌더 가능한 형태로 정리하는 계층이다.  
  - visible 수집, 분류, draw command 생성, overlay용 데이터 준비 등을 담당한다.
  
- `Resources/`
  - 버퍼, 텍스처, 셰이더, 상태 객체, 바인딩 슬롯 등 렌더 실행에 필요한 공용 자원을 관리하는 계층이다.

- `Visibility/`
  - frustum, occlusion, LOD, light culling 등 가시성 판단과 제출 최적화를 담당하는 계층이다.

### 6.1 `Render/Execute/`

#### `/Passes`

가장 작은 실행 단위를 둔다.  
Pass는 실제 draw, dispatch, fullscreen 작업을 담당하는 Leaf Node다.

#### `/Registry`

Pass, Pipeline, ViewMode, shader variant 매핑 규칙을 둔다.  
즉 어떤 Pipeline이 존재하는지, 각 Pipeline이 어떤 child node를 가지는지는 Registry 계층이 정의한다.

#### `/Context`

실행 중 공유되는 상태와 타입을 둔다.  
Context는 실행 노드들이 공통으로 참조하는 문맥이다.

#### `/Runner`

실행 트리를 실제로 순회하고 호출하는 실행기를 둔다.

### 6.2 `Render/Execute/Context`

#### `FRenderPipelineContext`

파이프라인 실행의 공통 문맥이다.  
전체 실행 흐름에서 공용으로 참조되는 상태를 담는다.

#### `FRenderSubmissionContext`

scene 및 overlay 수집 결과를 실행 단계로 전달하는 하위 문맥이다.  
Submission 단계에서 준비한 scene 데이터와 overlay 데이터를 실행 계층이 참조할 수 있도록 묶는다.

#### `FViewModeExecutionContext`

현재 ViewMode에 따른 분기 정보와 해석 문맥을 둔다.  
현재 active view mode, view mode registry, intermediate surfaces 등 ViewMode 실행에 필요한 정보를 담는다.


## 7. 실행 관련 데이터

| 타입 | 역할 |
|---|---|
| `FRenderPipelineDesc` | pipeline child node 정의 |
| `FRenderNodeRef` | child가 pass인지 pipeline인지 나타내는 참조 |
| `FRenderPipelineContext` | 실행 중 공유되는 renderer-owned 문맥 |
| `FViewModePassConfig` | view mode별 pass 활성화 규칙 |
| `FViewModePassDesc` | 특정 render pass에 대응하는 shader variant 정보 |

### 참고: Desc / Config / Preset 용어 구분

| 용어 | 의미 |
|---|---|
| `Desc` | 어떤 대상의 구조나 정체성을 설명하는 정의 값 |
| `Config` | 조건에 따라 무엇을 활성화할지 결정하는 설정 값 |
| `Preset` | 실행 시 바로 적용할 수 있는 고정 사용값 |
## 8. Deferred / Forward Pass Split

- deferred scene mesh pass는 `DeferredOpaquePass`를 사용한다.
- deferred decal scene pass는 `DeferredDecalPass`를 사용한다.
- `DeferredOpaquePass`는 `FViewModeSurfaces`에 intermediate surface를 기록한다.
- `DeferredDecalPass`는 base surface를 복사한 modified surface를 갱신한다.
- `DeferredLightingPass`는 위 intermediate surface 결과를 읽어 최종 viewport color를 합성한다.
- forward 경로는 별도 `ForwardDecalPass`를 두지 않고, 향후 `ForwardOpaquePass` 안에서 decal + lighting을 함께 처리한다.
- 현재 `ForwardPipeline` 계열은 `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline`, `ForwardSceneDepthPipeline`으로 구성된다.
- `ForwardWorldNormalPipeline`은 `ForwardOpaquePass`의 world normal shader variant가 viewport color에 직접 결과를 쓰는 direct output 경로를 사용한다.
- 이 경로는 world normal을 intermediate texture에 한 번 쓰고 다시 fullscreen pass에서 읽는 과정을 생략해, 추가 render target write/read bandwidth 비용을 피한다.
