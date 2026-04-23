# Render Overview

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-22 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.1 |


## Quick Links

- [Execution Structure](./01_render_execution_structure.md)
- [Resources](./02_render_resources.md)
- [Conventions: Naming](./03_render_conventions_naming.md)
- [Conventions: Structure](./04_render_conventions_structure.md)
- [Conventions: Binding](./05_render_conventions_binding.md)

## 1. 개요

이 문서는 Render 모듈 문서 세트의 범위와 읽기 순서를 정리한다.  
현재 렌더 구조는 `Scene`, `Submission`, `Execute`, `Resources`, `Visibility` 계층으로 나뉘며, 실행은 Registry에 등록된 Pass/Pipeline 트리를 기준으로 진행된다.

## 2. 문서 구성

| 문서 | 설명 |
|---|---|
| `render_overview.md` | 문서 세트의 범위와 읽기 순서를 설명한다. |
| `render_execution_structure.md` | 렌더 실행 구조, 실행 트리, Context, Registry, Pass 흐름을 설명한다. |
| `render_resources.md` | 렌더 패스가 사용하는 주요 리소스와 수명, 접근 패턴을 설명한다. |
| `render_conventions.md` | Render 모듈 전반의 구조 및 네이밍 규약을 정리한다. |
| `render_conventions_binding.md` | HLSL/C++ 바인딩 슬롯 규약과 관리 기준을 정리한다. |

## 3. 현재 렌더 구조 요약

현재 렌더 모듈은 다음 계층으로 구성된다.

- `Scene`
  - SceneProxy와 장면 렌더 데이터 관리
- `Submission`
  - scene / overlay 데이터를 수집하고 draw command로 변환
- `Execute`
  - pass, pipeline tree, registry, runner, 실행 context 관리
- `Resources`
  - 프레임 공용 버퍼, 셰이더, 상태, 바인딩 슬롯 관리
- `Visibility`
  - frustum, occlusion, LOD, tile-based light culling 관리

렌더 실행은 단일 함수에 하드코딩하지 않고,  
**Registry에 등록된 실행 트리**를 `RenderPipelineRunner`가 순회하는 방식으로 구성한다.

## 4. 현재 코드 기준 핵심 용어

| 용어 | 의미 |
|---|---|
| Pass | 실제 GPU 작업을 수행하는 최소 실행 단위 |
| Pipeline | Pass 또는 다른 Pipeline을 묶는 실행 조합 단위 |
| Registry | Pass, Pipeline, ViewMode별 실행 규칙을 등록·조회하는 계층 |
| Runner | 등록된 실행 트리를 실제로 순회하며 실행하는 계층 |
| Context | 실행 중 공유되는 상태와 참조를 담는 구조 |
| Draw Command | 실제 draw 제출 단위 |

## 5. 현재 코드 기준 반영 사항

문서 작성 시 아래 사항을 기준으로 유지한다.

- 파이프라인 정의 위치: `Render/Execute/Registry/RenderPipelineRegistry`
- ViewMode 분기 정의 위치: `Render/Execute/Registry/ViewModePassRegistry`
- 프레임 공용 리소스 타입명: `FFrameResources`
- intermediate surface 타입명: `FViewModeSurfaces`
- 실행 공통 문맥 타입명: `FRenderPipelineContext`

## 6. 권장 읽기 순서

| 순서 | 문서 | 목적 |
|---|---|---|
| 1 | `render_overview.md` | 문서 범위와 용어를 먼저 파악한다. |
| 2 | `render_execution_structure.md` | 전체 실행 구조와 파이프라인 흐름을 이해한다. |
| 3 | `render_resources.md` | pass 간에 오가는 리소스 구조를 이해한다. |
| 4 | `render_conventions.md` | 코드 확장 시 따라야 할 구조와 네이밍 규칙을 확인한다. |
| 5 | `render_conventions_binding.md` | 셰이더와 pass 추가 시 슬롯 규약을 확인한다. |

## 7. 정리

현재 Render 문서 세트는  
**실행 구조**, **리소스 구조**, **구조 규약**, **바인딩 규약**을 분리해 설명한다.

각 문서는 서로 다른 관심사를 다루지만,  
모두 동일한 렌더 실행 구조를 기준으로 작성되어야 한다.
