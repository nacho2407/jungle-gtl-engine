# Render Execution Structure

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-24 |
| 버전 | 1.0 |

## 1. 개요

> 조립형 렌더 파이프라인: 원하는 렌더 실행단위를 자유롭게 조합해 파이프라인을 만들자.

실행 단위는 실제 GPU 작업을 수행하는 단위인 `Pass`와, Pass 혹은 Pipeline들을 묶어 실행하는 단위인 `Pipeline`으로 나뉜다.

`Pass`와 `Pipeline`는 모두 렌더 노드로 취급한다. 따라서 `Pipeline` 내부에는 `Pass`뿐 아니라 다른 `Pipeline`도 들어갈 수 있으며, 전체 실행 구조는 트리로 표현된다.

이 문서의 목적은 렌더러가 어떤 실행 단위들로 구성되고, 그 단위들이 어떤 문맥과 순서로 실행되는지 설명하는 것이다.

## 2. 실행 구조 핵심

### 2.1 재사용 가능한 실행 단위: "Render Node"

렌더러는 큰 단일 함수가 아니라, 실행 단위인 "렌더 노드(Render Node)"를 조합해서 구성한다.

- Pass는 가장 작은 실행 단위다.
- Pipeline은 "Pass"를 묶는 조합 단위이자 실행 단위다.
- Pipeline 안에는 Pass 혹은 Pipeline이 일렬로 나열될 수 있다.
- Pass와 Pipeline은 모두 렌더 노드로 취급한다.
    - Pass: 실행의 주체가 되는 말단(Leaf) 노드이다.
    - Pipeline: Child Node 리스트를 갖는 조합 노드이다.

이러한 설계로 하여금 프로그래머가 원하는 실행 단위를 엮어 새로운 실행 단위를 조합해내거나 내부의 일부를 교체하는 등의 '렌더 실행단위 생성 · 편집'이 용이하다.

| 구분 | 역할 |
|---|---|
| Pass | 실제 draw, dispatch, fullscreen 작업 수행 |
| Pipeline | Pass/Pipeline을 묶어 실행 순서와 구조를 표현 |


### 2.2 실행 정책 및 실행 문맥(Context)

**실행 정책**은 렌더러가 가진다.  
렌더러는 어떤 경로를 실행할지, 어떤 순서로 실행할지, 현재 ViewMode에서 어떤 Pipeline을 선택할지를 결정한다.

실행 중 필요한 공유 상태인 **실행 문맥**은 `Context` 계층에서 관리한다.  
Context는 각 Pass와 Pipeline이 공통으로 참조하는 실행 상태이며 scene 정보, view mode 정보, 공용 상태 객체 등을 담는다.

## 3. 실행 흐름

전체적인 실행 흐름은 다음과 같다.

1. `Scene`과 `Submission`에서 렌더할 대상을 준비한다.
2. `Execute/Pipelines`에서 현재 프레임의 실행 트리를 구성한다.
3. `Runner`가 트리를 순회하며 실행한다.
4. 각 Leaf 단계는 `Pass`에서 실제 GPU 작업을 수행한다.
5. 실행 도중 필요한 상태는 `Context`가 제공한다.
6. Pass/ViewMode 매핑과 선택 규칙은 `Registry`가 관리한다.

즉, 렌더 대상 준비와 실행 구조는 분리되어 있으며, 실행 자체는 `Execute` 계층이 담당한다.

## 4. 디렉토리 구조

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

### 4.1 `Render/Execute/`

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

### 4.2 `Render/Execute/Context`

#### `FRenderPipelineContext`

파이프라인 실행의 공통 문맥이다.
전체 실행 흐름에서 공용으로 참조되는 상태를 담는다.

#### `FRenderSubmissionContext`

scene 및 overlay 수집 결과를 실행 단계로 전달하는 하위 문맥이다.
Submission 단계에서 준비한 scene 데이터와 overlay 데이터를 실행 계층이 참조할 수 있도록 묶는다.

#### `FViewModeExecutionContext`

현재 ViewMode에 따른 분기 정보와 해석 문맥을 둔다.
현재 active view mode, view mode registry, intermediate surfaces 등 ViewMode 실행에 필요한 정보를 담는다.

## 5. 실행 트리

현재 렌더 실행 구조는 다음 계층으로 구성된다.

- `Root Pipeline`
  - 최상위 실행 진입점
- `Scene / Overlay / PostProcess Pipeline`
  - 목적별 상위 Pipeline
- `ViewMode Pipeline`
  - `ScenePipeline` 내부의 ViewMode별 하위 Pipeline
- `Pass`
  - 실제 GPU 작업을 수행하는 Leaf Node

### 5.1 DefaultRootPipeline

기본 렌더 경로는 아래처럼 구성된다.

```text
DefaultRootPipeline
└─ ScenePipeline
   ├─ [Select ViewMode Pipeline]
   │  ├─ LitPipeline
   │  ├─ UnlitPipeline
   │  ├─ WorldNormalPipeline
   │  └─ SceneDepthPipeline
   └─ PostProcessPipeline
```

`ScenePipeline` 내부에서는 먼저 현재 ViewMode에 맞는 하위 Pipeline 하나를 선택해 실행한다.
이후 scene 결과에 대해 필요한 후처리 경로를 `PostProcessPipeline`에서 이어서 처리한다.

즉, `LitPipeline`, `UnlitPipeline`, `WorldNormalPipeline`, `SceneDepthPipeline`은
서로 병렬로 모두 실행되는 것이 아니라, 현재 ViewMode에 따라 **하나의 경로만 선택적으로 활성화**된다.

### 5.2 EditorRootPipeline

에디터 렌더 경로는 아래처럼 구성된다.

```text
EditorRootPipeline
├─ ScenePipeline
│  ├─ [Select ViewMode Pipeline]
│  │  ├─ LitPipeline
│  │  ├─ UnlitPipeline
│  │  ├─ WorldNormalPipeline
│  │  └─ SceneDepthPipeline
│  └─ PostProcessPipeline
└─ OverlayPipeline
   ├─ LightHitMapPass
   ├─ DebugLinePass
   ├─ OutlinePipeline
   ├─ OverlayBillboardPass
   ├─ GizmoPass
   └─ OverlayTextPass
```

`EditorRootPipeline`은 기본적인 scene 렌더 경로 위에,
에디터 전용 overlay 경로를 추가로 실행하는 구조다.

즉, scene 결과를 먼저 만든 뒤 그 위에 에디터 보조 요소를 순서대로 덧그린다.

### 5.3 ViewMode별 실행 경로

앞 절의 실행 트리는 **등록된 전체 구조**를 보여준다.
실제 프레임 실행에서는 이 중 현재 `ViewMode`에 대응하는 `ViewMode Pipeline`만 활성화된다.

즉 ViewMode는 개별 Pass를 직접 하나씩 고르는 개념이라기보다 `ScenePipeline` 내부에서 **어떤 ViewMode Pipeline 경로를 활성화할지 결정하는 기준**이다.

현재 기준 실행 경로는 다음과 같다.

```text
1. LitPipeline_-        : DepthPre -> LightCulling -> Opaque -> Decal -> Lighting -> PostProcess

2. UnlitPipeline        : DepthPre -> Opaque -> Decal -> PostProcess

3. WorldNormalPipeline  : DepthPre -> Opaque -> Decal -> NonLitView

4. SceneDepthPipeline   : DepthPre -> NonLitView
```

여기서 `LitPipeline_*`는 `Lit_Gouraud`, `Lit_Lambert`, `Lit_Phong` 등 Lit 계열 ViewMode가 공통으로 따르는 실행 경로를 의미한다.

또한 여기서 `NonLitView`는 `NonLitViewModePass`를 의미한다.

이처럼 실행 트리는 전체 후보 경로를 보관하고,
실제 프레임에서는 현재 ViewMode에 맞는 하위 Pipeline만 선택적으로 실행된다.

## 6. 실행 정의 데이터

렌더 실행 구조는 Pass와 Pipeline만으로 구성되지 않는다.  
실제 실행 경로를 정의하고 해석하기 위해, Registry가 참조하는 여러 정의 데이터가 함께 사용된다.

| 타입 | 역할 |
|---|---|
| `FRenderPassPreset` | pass별 리소스, 바인딩, 드로우 상태 적용값 묶음 |
| `FRenderPassDrawPreset` | depth, blend, rasterizer, topology 등 draw 상태 preset |
| `FRenderPipelineDesc` | pipeline 실행 트리 정의 |
| `FViewModePassConfig` | ViewMode별 pass enable/disable 및 shader 구성 |
| `FViewModePassDesc` | 특정 render pass에 대응하는 shader variant 정보 |

### 참고: Preset / Desc / Config 용어 구분

| 용어 | 의미 |
|---|---|
| `Desc` | 어떤 대상의 구조나 정체성을 설명하는 정의 데이터 |
| `Config` | 조건에 따라 무엇을 활성화할지 결정하는 설정 데이터 |
| `Preset` | 실행 시 바로 적용할 수 있는 고정 사용값 묶음 |
