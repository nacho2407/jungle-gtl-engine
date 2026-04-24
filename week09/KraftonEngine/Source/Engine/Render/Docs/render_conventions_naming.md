# Render Conventions: Naming

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-24 |
| 버전 | 1.0 |

## 1. 기본 원칙

- 이름은 역할이 바로 드러나게 짓는다.
- 문서와 코드 모두 현재 코드 기준 타입명을 사용한다.
- `Pass`, `Pipeline`, `Context`, `Resources`는 접미사로 역할을 구분한다.

## 2. Pass Naming

- 실제 GPU 작업 단위는 `Pass` 접미사를 사용한다.
- 이름은 수행 작업이 바로 드러나게 짓는다.

예:
- `DepthPrePass`
- `OpaquePass`
- `LightingPass`
- `OverlayTextPass`
- `NonLitViewModePass`

## 3. Pipeline Naming

- 실행 경로를 묶는 단위는 `Pipeline` 접미사를 사용한다.
- 최상위 진입점일 때만 `RootPipeline`을 사용한다.

예:
- `ScenePipeline`
- `OverlayPipeline`
- `PostProcessPipeline`
- `PresentPipeline`
- `DefaultRootPipeline`
- `EditorRootPipeline`
- `LitPipeline`
- `UnlitPipeline`
- `WorldNormalPipeline`
- `SceneDepthPipeline`
- `OutlinePipeline`

## 4. Context Naming

- 실행 중 공유되는 문맥 타입은 `Context` 접미사를 사용한다.
- 상위 공통 문맥과 하위 역할 문맥을 이름으로 구분한다.

예:
- `FRenderPipelineContext`
- `FViewModeExecutionContext`
- `FRenderSubmissionContext`
- `FCollectRenderContext`
- `FSceneView`

## 5. Resource Naming

- 프레임 공용 리소스는 `Resources`
- intermediate target 묶음은 `Surfaces`
- viewport 출력 묶음은 `RenderTargets`
- constant buffer payload는 `CBData`

예:
- `FFrameResources`
- `FViewModeSurfaces`
- `FViewportRenderTargets`
- `FFrameCBData`
- `FPerObjectCBData`
- `FFogCBData`

## 6. 실행 정의 데이터 Naming

| 용어 | 의미 |
|---|---|
| `Desc` | 구조나 정체성을 설명하는 정의 데이터 |
| `Config` | 조건에 따라 무엇을 활성화할지 결정하는 설정 데이터 |
| `Preset` | 실행 시 바로 적용할 수 있는 고정 사용값 묶음 |

예:
- `FRenderPipelineDesc`
- `FViewModePassConfig`
- `FViewModePassDesc`
- `FRenderPassPreset`
- `FRenderPassDrawPreset`
