# Render

> 문서 버전: 1.0  
> 최종 수정일: 2026-04-24

## 세부 문서 바로가기

- [실행 구조: Execution Structure](./render_execution_structure.md)
- [자원: Resources](./render_resources.md)
- [이름 규약: Naming Convention](./render_conventions_naming.md)
- [바인딩 규약: Binding Convention](./render_conventions_binding.md)

## 설계 요약

> 조립형 렌더 파이프라인: 원하는 렌더 실행단위를 자유롭게 조합해 파이프라인을 만들자.

## 모듈 구성

렌더 모듈은 다음 계층으로 구성된다.

- `Scene`
  - SceneProxy와 장면 렌더 데이터 관리
- `Submission`
  - Scene / Overlay 데이터를 수집하고 draw command로 변환
- `Execute`
  - pass 실행, pipeline tree 순회, registry 기반 실행 선택, 실행 context 관리
- `Resources`
  - 프레임 공용 버퍼, 렌더 상태, 셰이더, 바인딩 슬롯 등 렌더 공용 자원 관리
- `Visibility`
  - frustum, occlusion, LOD, light culling 등 가시성 판단과 화면 제출 최적화 관리

렌더 실행은 단일 함수에 하드코딩하지 않고,  
**Registry에 등록된 최상위(Root) 실행 단위**를 `FRenderPipelineRunner`가 순회하는 방식으로 구성한다.

## 핵심 용어

| 용어 | 의미 |
|---|---|
| Pass | 실제 GPU 작업을 수행하는 최소 실행 단위 |
| Pipeline | Pass 또는 다른 Pipeline을 묶는 실행 조합 단위 |
| Registry | Pass, Pipeline, ViewMode별 실행 규칙을 등록 · 조회하는 계층 |
| Runner | 등록된 실행 트리를 실제로 순회하며 실행하는 계층 |
| Context | 실행 중 공유되는 상태와 참조를 담는 구조 |
| Draw Command | 실제 draw 제출 단위 |

