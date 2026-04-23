# Render Conventions: Naming

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-24 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.0 |

## 1. 목적

이 문서는 Render 모듈에서 사용하는 주요 타입과 실행 단위의 네이밍 규칙을 정리한다.  
대상은 다음 네 가지다.

- Pass
- Pipeline
- Context
- Resource

이 규칙의 목적은 이름만 보고 역할과 범위를 빠르게 파악할 수 있게 하는 것이다.

## 2. 기본 원칙

### 2.1 역할이 이름에 드러나야 한다

이름은 내부 구현보다 **역할과 사용 위치**를 먼저 드러내야 한다.

예:

- `DepthPrePass`
- `OpaquePass`
- `OverlayPipeline`
- `FRenderPipelineContext`
- `FFrameResources`

### 2.2 현재 코드 기준 이름을 유지한다

문서와 코드 모두 현재 코드 기준 타입명을 사용한다.  
과거 이름이나 리팩토링 이전 이름은 문서 본문에서 기본 용어로 사용하지 않는다.

예:

- `FFrameRenderResources` 대신 `FFrameResources`
- `FSceneViewModeSurfaces` 대신 `FViewModeSurfaces`

### 2.3 조합 단위와 실행 단위를 구분한다

- 실제 GPU 작업을 수행하는 단위는 `Pass`
- Pass 또는 다른 Pipeline을 묶는 단위는 `Pipeline`

이 차이가 이름에서 바로 드러나야 한다.

## 3. Pass Naming

### 3.1 규칙

| 규칙 | 설명 |
|---|---|
| 실제 렌더 작업을 수행하는 타입은 `Pass` 접미사를 사용한다. |
| 이름은 수행 작업이 바로 드러나게 짓는다. |
| 범용 이름보다 기능 중심 이름을 우선한다. |

### 3.2 예시

| 좋은 예 | 이유 |
|---|---|
| `DepthPrePass` | depth pre-pass 역할이 분명하다. |
| `OpaquePass` | opaque 렌더 단계임이 바로 드러난다. |
| `LightingPass` | 라이팅 단계임이 명확하다. |
| `OverlayTextPass` | overlay 계층의 text 렌더 pass임이 드러난다. |
| `NonLitViewModePass` | 특정 view mode 표시용 pass임이 드러난다. |

### 3.3 피할 이름

| 피할 예 | 이유 |
|---|---|
| `MainPass` | 무엇을 하는지 불명확하다. |
| `FinalPass` | 맥락이 없으면 역할이 모호하다. |
| `ExtraPass` | 기능을 설명하지 못한다. |

## 4. Pipeline Naming

### 4.1 규칙

| 규칙 | 설명 |
|---|---|
| Pass/Pipeline 조합 단위는 `Pipeline` 접미사를 사용한다. |
| 실행 경로의 목적이나 묶음 기준이 이름에 드러나야 한다. |
| 최상위 진입점일 때만 `RootPipeline`을 사용한다. |

### 4.2 예시

| 이름 | 의미 |
|---|---|
| `ScenePipeline` | scene 렌더 경로를 묶는 pipeline |
| `OverlayPipeline` | overlay 렌더 경로를 묶는 pipeline |
| `PresentPipeline` | present 관련 실행 경로를 묶는 pipeline |
| `DefaultRootPipeline` | 기본 실행 진입점 |
| `EditorRootPipeline` | editor 실행 진입점 |

### 4.3 Root 사용 규칙

| 규칙 | 설명 |
|---|---|
| `RootPipeline`은 외부 실행 진입점에만 사용한다. |
| 재사용 가능한 중간 pipeline에는 `Root`를 붙이지 않는다. |
| 의미 없는 단독 이름 `RootPipeline`은 사용하지 않는다. |

### 4.4 ViewMode 가지 이름

현재 코드에서 일부 pipeline child는 별도 `*Pipeline` 접미사 없이 의미 이름만 사용한다.

예:

- `Lit`
- `Unlit`
- `WorldNormal`
- `SceneDepth`

이 경우는 **view mode 가지 이름**으로 이해하고,  
문서에서는 “ScenePipeline 내부 분기”라는 맥락과 함께 설명한다.

## 5. Context Naming

### 5.1 규칙

| 규칙 | 설명 |
|---|---|
| 실행 중 공유되는 문맥 타입은 `Context` 접미사를 사용한다. |
| 상위 공통 문맥과 하위 역할 문맥을 이름으로 구분한다. |
| view 관련 문맥은 `View` 의미를 유지한다. |

### 5.2 예시

| 이름 | 의미 |
|---|---|
| `FRenderPipelineContext` | 전체 패스 실행 공통 문맥 |
| `FViewModeExecutionContext` | view mode 관련 하위 문맥 |
| `FRenderSubmissionContext` | submission 결과 전달 문맥 |
| `FCollectRenderContext` | collect 단계 문맥 |
| `FSceneView` | 카메라/행렬/view 설정 문맥 |

### 5.3 네이밍 기준

- 전역 실행 공통 문맥
  - `RenderPipelineContext`
- 특정 관심사 하위 문맥
  - `ViewModeExecutionContext`
  - `RenderSubmissionContext`
- 단순 데이터 묶음이 아닌 **실행 시 참조되는 문맥**일 때 `Context`를 쓴다.

## 6. Resource Naming

### 6.1 규칙

| 규칙 | 설명 |
|---|---|
| 프레임 공용 리소스는 `Resources` 접미사를 사용한다. |
| intermediate target 묶음은 `Surfaces`를 사용한다. |
| viewport 출력 묶음은 `RenderTargets`를 사용한다. |
| constant buffer payload는 `CBData` 접미사를 사용한다. |

### 6.2 예시

| 이름 | 의미 |
|---|---|
| `FFrameResources` | 프레임 전반 공용 리소스 |
| `FViewModeSurfaces` | view mode intermediate surface 묶음 |
| `FViewportRenderTargets` | viewport 출력 타깃 묶음 |
| `FFrameCBData` | frame constant buffer payload |
| `FPerObjectCBData` | per-object constant buffer payload |
| `FFogCBData` | fog pass용 constant buffer payload |

### 6.3 Resource와 Context 구분

| 구분 | 설명 |
|---|---|
| `Resources` | 실제 GPU 자원 또는 자원 묶음 |
| `Context` | 실행 중 참조되는 상태와 포인터/참조 묶음 |

예를 들어 `FFrameResources`는 resource 소유에 가깝고,  
`FRenderPipelineContext`는 resource에 대한 참조를 포함하는 실행 문맥이다.

## 7. 문서 작성 시 이름 표기 규칙

- 타입명은 코드와 동일하게 표기한다.
- 축약보다 실제 타입명을 우선한다.
- 접미사는 의미를 유지한다.
  - `Pass`
  - `Pipeline`
  - `Context`
  - `Resources`
  - `Surfaces`
  - `RenderTargets`
  - `CBData`

## 8. 정리

Render 모듈의 네이밍은 다음 기준으로 유지한다.

1. 역할이 이름에 바로 드러나야 한다.
2. Pass / Pipeline / Context / Resource를 접미사로 구분한다.
3. 문서와 구현 모두 현재 코드 기준 타입명을 사용한다.
