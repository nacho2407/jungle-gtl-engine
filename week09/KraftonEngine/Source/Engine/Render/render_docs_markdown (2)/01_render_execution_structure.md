# Render Execution Structure

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-22 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.1 |

## 1. 개요

현재 렌더 구조는 재사용 가능한 실행 단위를 조합하는 방식으로 구성된다.

가장 작은 실행 단위는 `Pass`이며,  
여러 Pass 또는 다른 Pipeline을 묶는 상위 실행 단위는 `Pipeline`이다.

이 둘은 모두 렌더 실행 노드로 취급되며, 전체 렌더 경로는 트리 형태로 구성된다.  
실제 실행 트리는 `FRenderPipelineRegistry`에 등록되며, 실행은 `FRenderPipelineRunner`가 담당한다.

## 2. 설계 원칙

### 2.1 Pass와 Pipeline의 분리

- `Pass`
  - 실제 GPU 작업을 수행하는 leaf node
  - draw, fullscreen draw, post-process, dispatch 작업 수행
- `Pipeline`
  - Pass 또는 다른 Pipeline을 순서대로 묶는 조합 단위
  - 실행 경로를 재사용 가능한 구조로 제공

| 구분 | 역할 |
|---|---|
| Pass | 실제 GPU 작업 수행 |
| Pipeline | Pass/Pipeline 조합 및 실행 순서 정의 |

### 2.2 실행 정책과 실행 문맥의 분리

실행 정책은 Registry와 Runner가 담당한다.

- `FRenderPassRegistry`
  - pass 인스턴스와 pass preset 관리
- `FRenderPipelineRegistry`
  - pipeline tree 등록 및 조회
- `FViewModePassRegistry`
  - view mode별 pass enable/disable 및 shader variant 관리
- `FRenderPipelineRunner`
  - 현재 view mode와 registry 정보를 기준으로 실행 트리를 순회

실행 중 공유되는 상태는 `FRenderPipelineContext`가 담당한다.

## 3. 주요 구성 요소

### 3.1 Pass Base

모든 pass는 `FRenderPass` 인터페이스를 따른다.

기본 흐름은 다음과 같다.

1. `PrepareInputs`
2. `PrepareTargets`
3. `SubmitDrawCommands`

필요 시 pass 내부에서 draw command를 생성하거나, 이미 준비된 draw command를 제출한다.

### 3.2 Registry

현재 렌더 구조는 Registry 중심으로 구성된다.

| 타입 | 역할 |
|---|---|
| `FRenderPassRegistry` | pass 인스턴스와 `FRenderPassPreset` 등록/조회 |
| `FRenderPipelineRegistry` | pipeline tree 등록/조회 |
| `FViewModePassRegistry` | view mode별 pass 사용 여부와 shader variant 관리 |

### 3.3 Runner

`FRenderPipelineRunner`는 registry에 등록된 pipeline tree를 재귀적으로 순회한다.

- child가 pipeline이면 하위 pipeline을 재귀 실행
- child가 pass이면 현재 조건을 검사한 뒤 실행
- 실행 조건은 주로 `EViewMode`, `FViewModePassRegistry`, pass 활성 여부를 기준으로 결정

즉, 트리 구조는 Registry가 정의하고,  
실제 순회와 호출은 Runner가 담당한다.

## 4. 디렉토리 구조

```text
Render/
├─ Execute/
│  ├─ Context/
│  ├─ Passes/
│  ├─ Registry/
│  └─ Runner/
├─ Resources/
├─ Scene/
├─ Submission/
└─ Visibility/
```

## 5. `Render/Execute/` 구성

### 5.1 `Context/`

패스 실행 중 공통으로 참조하는 문맥 타입을 둔다.

주요 타입:

- `FRenderPipelineContext`
- `FSceneView`
- `FViewportRenderTargets`
- `FViewModeSurfaces`

### 5.2 `Passes/`

실제 GPU 작업을 수행하는 leaf pass를 둔다.

예:

- `DepthPrePass`
- `OpaquePass`
- `DecalPass`
- `LightingPass`
- `HeightFogPass`
- `FXAAPass`
- `OutlinePass`

### 5.3 `Registry/`

pass, pipeline, view mode별 실행 구성을 등록·조회한다.

현재 코드에서 파이프라인 정의는 별도 `Pipelines/` 폴더에 두지 않고,  
`FRenderPipelineRegistry` 안에 직접 등록한다.

### 5.4 `Runner/`

등록된 pipeline tree를 실제로 순회하고 실행한다.

## 6. 실행 문맥

### 6.1 `FRenderPipelineContext`

패스 실행에 필요한 공통 문맥이다.

대표적으로 다음 정보를 포함한다.

- 현재 scene view
- 현재 viewport targets
- 현재 scene 참조
- D3D 장치 및 context
- 프레임 공용 리소스
- draw command list
- visibility 관련 상태
- view mode 실행 문맥
- submission 결과

### 6.2 `FViewModeExecutionContext`

현재 view mode 실행에 필요한 하위 문맥이다.

- `Registry`
- `Surfaces`
- `ActiveViewMode`

### 6.3 `FRenderSubmissionContext`

수집 결과를 실행 단계에 넘기는 하위 문맥이다.

- `SceneData`
- `OverlayData`

## 7. 실행 준비 흐름

렌더러는 대략 다음 순서로 동작한다.

1. `BeginCollect`
2. `CollectWorld`
3. `CollectGrid`
4. `CollectOverlayText`
5. `CollectDebugRender`
6. `BeginFrame`
7. `PreparePipelineExecution`
8. `ExecuteRootPipeline`
9. `ExecutePresentPass`
10. `FinalizePipelineExecution`

즉, 수집과 실행은 분리되어 있으며,  
실행 단계는 이미 준비된 수집 결과를 기반으로 진행된다.

## 8. 현재 파이프라인 트리

### 8.1 DefaultRootPipeline

```text
DefaultRootPipeline
└─ ScenePipeline
   ├─ Lit
   │  ├─ DepthPrePass
   │  ├─ LightCullingPass
   │  ├─ OpaquePass
   │  ├─ DecalPass
   │  └─ LightingPass
   ├─ Unlit
   │  ├─ DepthPrePass
   │  ├─ OpaquePass
   │  └─ DecalPass
   ├─ WorldNormal
   │  ├─ DepthPrePass
   │  ├─ OpaquePass
   │  ├─ DecalPass
   │  └─ NonLitViewModePass
   ├─ SceneDepth
   │  ├─ DepthPrePass
   │  └─ NonLitViewModePass
   └─ PostProcessPipeline
      ├─ HeightFogPass
      └─ FXAAPass
```

### 8.2 EditorRootPipeline

```text
EditorRootPipeline
├─ ScenePipeline
├─ OverlayPipeline
│  ├─ LightHitMapPass
│  ├─ DebugLinePass
│  ├─ Outline
│  │  ├─ SelectionMaskPass
│  │  └─ OutlinePass
│  ├─ OverlayBillboardPass
│  ├─ GizmoPass
│  └─ OverlayTextPass
└─ PresentPipeline
   └─ PresentPass
```

## 9. ViewMode별 실행 경로

### 9.1 Lit 계열

대상:

- `Lit_Gouraud`
- `Lit_Lambert`
- `Lit_Phong`

실행 경로:

```text
DepthPre -> LightCulling -> Opaque -> Decal -> Lighting -> HeightFog -> FXAA
```

### 9.2 Unlit / Wireframe

실행 경로:

```text
DepthPre -> Opaque -> Decal -> HeightFog -> FXAA
```

`Wireframe`은 별도 pipeline이 아니라,  
draw command 구성 과정에서 rasterizer 상태를 wireframe으로 덮어쓰는 방식으로 처리한다.

### 9.3 WorldNormal

실행 경로:

```text
DepthPre -> Opaque -> Decal -> NonLitViewModePass
```

### 9.4 SceneDepth

실행 경로:

```text
DepthPre -> NonLitViewModePass
```

## 10. Submission과 Execute의 관계

`Submission`은 장면 데이터를 수집하고 draw command로 정리한다.  
`Execute`는 이를 기반으로 pass를 실행하고 실제 GPU 작업을 제출한다.

| 계층 | 역할 |
|---|---|
| `Scene` | SceneProxy와 원천 렌더 데이터 제공 |
| `Submission/Collect` | primitive / light / overlay 데이터 수집 |
| `Submission/Command` | draw command 생성 및 정렬 |
| `Execute/Passes` | pass별 입력/타깃 준비 후 draw 제출 |

## 11. 정리

현재 렌더 구조는 다음과 같이 요약할 수 있다.

> 렌더러는 Registry에 등록된 Pass/Pipeline 트리를, 현재 ViewMode와 실행 문맥에 따라 조건적으로 순회하며 실행한다.
