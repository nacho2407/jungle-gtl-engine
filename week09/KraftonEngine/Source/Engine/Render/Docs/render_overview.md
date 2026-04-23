# Render Overview

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-22 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-23 |
| 상태 | Draft |
| 버전 | 1.0 |

## 1. 문서 구성

| 문서 | 설명 |
|---|---|
| `render_execution_structure.md` | 렌더 실행 구조, 실행 트리, Context, 주요 실행 단계 설명 |
| `render_resources.md` | 렌더 패스가 사용하는 주요 리소스의 종류, 소유, 수명, 접근 방식 설명 |
| `render_conventions.md` | 바인딩 규약과 네이밍 규칙을 표 중심으로 정리한 컨벤션 문서 |

각 문서는 서로 다른 관심사를 다루지만, 모두 같은 렌더 구조를 설명한다. 따라서 문서 간 용어와 이름은 동일하게 유지해야 한다.

## 2. TODO

- `render_execution_structure.md`에 Editor 경로 예시를 더 보강할지 검토
- `render_resources.md`에 pass-local temporary resource 사례를 추가할지 검토
- `render_conventions.md`의 바인딩 규약 표를 실제 슬롯 배치와 동기화할지 검토
