# Render Conventions: Structure

| 항목 | 내용 |
|---|---|
| 작성자 | 김연하 |
| 작성일 | 2026-04-24 |
| 최종 수정자 | 김연하 |
| 최종 수정일 | 2026-04-24 |
| 상태 | Draft |
| 버전 | 1.0 |

## 1. 목적

이 문서는 Render 모듈의 구조적 책임 분리 규칙을 정리한다.  
대상은 다음 다섯 가지다.

- Pass
- Pipeline
- Registry
- Runner
- Submission

핵심 목표는 실행 구조가 커져도 역할이 섞이지 않게 유지하는 것이다.

## 2. 기본 원칙

### 2.1 실행 단위와 실행 정책을 분리한다

- 실제 GPU 작업은 `Pass`
- 실행 경로 조합은 `Pipeline`
- 실행 구조 정의는 `Registry`
- 실행 순회와 호출은 `Runner`

이 네 역할은 서로 섞지 않는다.

### 2.2 수집과 실행을 분리한다

- 무엇을 그릴지 준비하는 일은 `Submission`
- 어떻게 실행할지 결정하고 GPU에 제출하는 일은 `Execute`

즉, 장면 데이터 수집과 렌더 패스 실행은 별도 단계로 유지한다.

## 3. Pass 책임 규칙

### 3.1 Pass의 역할

Pass는 실제 GPU 작업을 수행하는 최소 실행 단위다.

Pass는 다음 책임만 가진다.

- 필요한 입력 준비
- 필요한 target 바인딩
- 자신의 draw/dispatch/fullscreen 작업 제출

### 3.2 Pass가 하지 말아야 할 일

| 금지 항목 | 이유 |
|---|---|
| 전체 파이프라인 순서를 직접 결정 | 상위 실행 구조와 책임이 섞인다. |
| 다른 pass의 존재를 전제로 실행 흐름을 강하게 하드코딩 | 재사용성과 독립성이 떨어진다. |
| registry 역할까지 겸함 | 정의와 실행이 섞인다. |

### 3.3 Pass 설계 기준

- leaf node로 유지한다.
- 가능하면 자신의 입력/출력 범위 안에서 닫히게 만든다.
- 상위 흐름 제어는 runner와 registry에 맡긴다.

## 4. Pipeline 책임 규칙

### 4.1 Pipeline의 역할

Pipeline은 Pass 또는 다른 Pipeline을 묶는 조합 단위다.

책임은 다음과 같다.

- 실행 경로를 재사용 가능한 묶음으로 구성
- 장면/오버레이/프레젠트처럼 상위 흐름을 논리적으로 분리
- view mode별 가지를 표현

### 4.2 Pipeline이 하지 말아야 할 일

| 금지 항목 | 이유 |
|---|---|
| 개별 draw 제출 직접 수행 | leaf 작업은 pass 책임이다. |
| 세부 GPU 상태 변경까지 직접 수행 | 실제 작업 단위와 조합 단위가 섞인다. |
| registry 없이 임의로 구조를 흩뿌림 | 실행 구조 추적이 어려워진다. |

### 4.3 현재 코드 기준 규칙

현재 코드에서는 pipeline을 별도 `Pipelines/` 폴더에 두지 않고,  
`FRenderPipelineRegistry`에 실행 트리로 등록한다.

따라서 pipeline은 **클래스보다 구조 정의**에 가깝게 다룬다.

## 5. Registry 책임 규칙

### 5.1 Registry의 역할

Registry는 실행 구조와 선택 규칙의 source of truth다.

현재 주요 registry는 다음과 같다.

| 타입 | 역할 |
|---|---|
| `FRenderPassRegistry` | pass 인스턴스와 pass preset 등록/조회 |
| `FRenderPipelineRegistry` | pipeline tree 등록/조회 |
| `FViewModePassRegistry` | view mode별 pass enable/disable 및 shader variant 관리 |

### 5.2 Registry가 담당해야 하는 것

- 어떤 pass가 존재하는가
- 어떤 pipeline tree가 존재하는가
- 어떤 view mode가 어떤 pass를 사용하는가
- 어떤 shader variant가 연결되는가

### 5.3 Registry가 하지 말아야 할 일

| 금지 항목 | 이유 |
|---|---|
| 실제 draw 실행 | registry는 정의 계층이다. |
| frame별 동적 실행 상태 보관 | 실행 문맥과 책임이 섞인다. |
| pass 내부 로직까지 직접 수행 | 등록과 실행이 분리되어야 한다. |

## 6. Runner 책임 규칙

### 6.1 Runner의 역할

Runner는 registry에 등록된 실행 트리를 실제로 순회하고 실행한다.

주요 책임은 다음과 같다.

- pipeline child 순회
- child가 pass인지 pipeline인지 구분
- 현재 조건에 따라 실행 여부 판단
- pass 실행 호출

### 6.2 Runner의 판단 기준

Runner는 보통 아래 정보를 활용한다.

- 현재 `EViewMode`
- `FViewModePassRegistry`
- 현재 실행 context
- pass 활성 여부

### 6.3 Runner가 하지 말아야 할 일

| 금지 항목 | 이유 |
|---|---|
| pass 구현 세부 로직 소유 | 실행기와 작업 단위가 섞인다. |
| 구조 정의를 자기 내부에 하드코딩 | registry와 역할이 중복된다. |
| submission 수집까지 수행 | 실행과 수집이 섞인다. |

## 7. Submission 책임 규칙

### 7.1 Submission의 역할

Submission은 장면 데이터를 실제 렌더 가능한 형태로 정리하는 단계다.

현재 역할은 크게 둘로 나뉜다.

| 계층 | 역할 |
|---|---|
| `Collect` | primitive / light / overlay 데이터를 수집 |
| `Command` | draw command 생성 및 정렬 |

### 7.2 Submission이 담당하는 것

- scene proxy 수집
- light 수집
- overlay/debug 데이터 수집
- draw command 생성
- pass 실행 전에 필요한 제출 단위 준비

### 7.3 Submission이 하지 말아야 할 일

| 금지 항목 | 이유 |
|---|---|
| pass 실행 순서 결정 | execute 계층 책임이다. |
| registry 구조 정의 | submission 계층 관심사가 아니다. |
| 후처리 pass 로직 직접 수행 | 실행 계층과 역할이 다르다. |

## 8. 계층 간 관계 규칙

### 8.1 권장 흐름

```text
Scene
 -> Submission/Collect
 -> Submission/Command
 -> Execute/Registry
 -> Execute/Runner
 -> Execute/Passes
```

### 8.2 역할 요약

| 계층 | 핵심 질문 |
|---|---|
| Submission | 무엇을 그릴까? |
| Registry | 어떤 구조로 실행할까? |
| Runner | 지금 무엇을 실행할까? |
| Pass | 실제로 어떻게 그릴까? |

## 9. 문서 작성 규칙

구조 문서에서는 다음을 명확히 구분해 적는다.

- Pass인가 Pipeline인가
- 정의 계층인가 실행 계층인가
- 수집 단계인가 실행 단계인가
- 고정 구조인가 프레임별 동적 상태인가

이 구분이 흐려지면 문서와 코드 모두 빠르게 복잡해진다.

## 10. 정리

Render 모듈 구조 규약의 핵심은 다음과 같다.

1. Pass는 leaf 작업만 담당한다.
2. Pipeline은 실행 경로 조합만 담당한다.
3. Registry는 구조와 선택 규칙을 관리한다.
4. Runner는 등록된 구조를 순회하며 실행한다.
5. Submission은 수집과 draw command 준비를 담당한다.
