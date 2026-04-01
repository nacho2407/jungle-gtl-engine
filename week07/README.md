# GameTechLab W4_Jungle_Team2

Windows 환경에서 동작하는 `Win32 + DirectX 11 + ImGui` 기반의 3D 에디터/엔진 프로젝트입니다.  
이번 리포지토리에는 두 가지 실행 형태가 함께 들어 있습니다.

- `Editor` 모드: 씬 편집, 다중 뷰포트, 선택/기즈모, 씬 저장 및 복원
- `ObjViewer` 모드: `.obj` 메시를 빠르게 검토하기 위한 전용 뷰어

이 문서는 코드와 첨부 발표 자료를 기준으로, 실제 구현이 확인된 기능만 정리한 설명문서입니다.

## 프로젝트 개요

이 프로젝트는 단순 렌더링 데모가 아니라, 다음 요소를 직접 구현한 소형 편집기 엔진에 가깝습니다.

- 커스텀 `UObject` 계층과 RTTI
- `FName` 기반 이름 관리
- 월드/액터/컴포넌트 구조
- DirectX 11 렌더링 파이프라인
- ImGui 기반 편집 UI
- OBJ/MTL 로더와 정적 메시 렌더링
- JSON 씬 저장/복원
- OBJ 바이너리 베이킹 및 캐시 로딩

## 핵심 기능

### 1. 에디터 모드

에디터 모드는 씬을 직접 배치하고 편집하는 데 초점이 맞춰져 있습니다.

- 4분할 뷰포트 지원
  - 좌상단 `Perspective`
  - 우상단 `Top`
  - 좌하단 `Front`
  - 우하단 `Right`
- 스플리터 드래그로 뷰포트 크기 조절
- 1개 뷰포트 전체화면 모드와 4분할 모드 전환
- 뷰포트별 호버/포커스 상태 관리
- 뷰포트 설정 저장
  - 스플리터 비율
  - 활성 뷰포트 수
  - 단일 뷰포트 인덱스

### 2. 카메라와 뷰포트 조작

- Perspective 뷰 카메라 이동 및 회전
- Orthographic 뷰 지원
- 직교 뷰에서 팬/줌 조작
- 마우스 휠 기반 FOV 또는 Ortho Height 조절
- 포커스된 뷰포트 기준 이동 속도 조절
- Perspective 카메라 상태 저장/복원
  - 위치
  - 회전
  - FOV
  - Near/Far Clip

### 3. 씬 편집 기능

- 월드의 액터 목록 표시
- 선택된 액터/컴포넌트의 속성 편집
- 프로퍼티 위젯 자동 렌더링
- 액터 선택
  - 단일 선택
  - `Shift` 추가 선택
  - `Ctrl` 토글 선택
- 박스 선택
- 기즈모 기반 편집
  - Translate
  - Rotate
  - Scale
- 기즈모 월드/로컬 모드 전환
- 선택 액터 삭제

### 4. 에디터 UI

ImGui 기반으로 아래 패널들이 분리되어 있습니다.

- `Scene Manager`
  - 액터 아웃라이너
  - 씬 저장/불러오기
- `Property`
  - 선택 오브젝트의 속성 수정
  - StaticMesh 경로 콤보 선택
- `Material Editor`
  - StaticMesh의 섹션별 머티리얼 확인
  - 슬롯별 머티리얼 교체
  - 색상/스칼라/텍스처 정보 확인
- `Console`
  - stat 명령 처리
- `Stat Profiler`
  - CPU/GPU 통계 테이블
- `Viewport Settings`
  - Show Flags
  - Grid 설정
  - 카메라 이동 속도

### 5. StaticMesh 렌더링

이번 주차에서 가장 큰 변화 중 하나는 OBJ 기반 정적 메시 렌더링입니다.

- `UStaticMesh`와 `UStaticMeshComponent` 추가
- OBJ를 렌더링용 정적 메시 데이터로 변환
- 섹션 단위 인덱스 관리
- 섹션별 머티리얼 슬롯 연결
- 로컬/월드 AABB 계산
- 메시 단위 레이캐스트
- 프로퍼티 창에서 StaticMesh 에셋 지정 가능

### 6. OBJ / MTL 파이프라인

OBJ 로더는 텍스트 파일을 바로 렌더링하지 않고, 중간 데이터를 거쳐 최종 메시로 변환합니다.

- OBJ 파싱 지원 항목
  - `v`
  - `vt`
  - `vn`
  - `mtllib`
  - `usemtl`
  - `f`
- Raw 데이터에서 Cooked StaticMesh로 변환
- 머티리얼 이름 기준 슬롯 생성
- 로컬 바운드 계산
- 로딩 옵션에 따라 단위 큐브 기준 정규화 가능

MTL 파서는 다음 정보를 읽습니다.

- `newmtl`
- `Ka`
- `Kd`
- `Ks`
- `Ke`
- `Ns`
- `d`
- `illum`
- `map_Kd`
- `map_Ka`
- `map_Ks`
- `map_bump` / `bump`

텍스처 경로는 파싱 시점에 정규화되고, 렌더링 시 SRV로 연결됩니다.

### 7. 다중 재질 처리

정적 메시 하나에 여러 재질을 연결할 수 있습니다.

- `usemtl` 기준 슬롯 생성
- 섹션별 `MaterialSlotIndex` 유지
- 메시 컴포넌트의 override material 배열 보유
- Material Editor에서 섹션별 머티리얼 교체 가능
- 머티리얼이 없으면 기본 재질/기본 흰색 텍스처로 대체

즉, "머티리얼 에디터"라기보다 "섹션별 머티리얼 확인 및 교체 UI"에 가깝습니다.

### 8. 렌더링 파이프라인

렌더링은 수집과 실행이 분리된 구조입니다.

- `RenderCollector`
  - 월드, 선택 오브젝트, 그리드, 기즈모 수집
- `RenderBus`
  - 패스별 커맨드 정리
- `Renderer`
  - 실제 GPU 상태 설정 및 드로우

에디터 렌더링에서 확인되는 요소는 다음과 같습니다.

- 일반 오브젝트 렌더링
- StaticMesh 렌더링
- Grid 렌더링
- Billboard Text 렌더링
- SubUV 렌더링
- 기즈모 렌더링
- 선택 마스크 + 포스트프로세스 아웃라인

View Mode는 다음을 지원합니다.

- `Lit`
- `Unlit`
- `Wireframe`

Show Flags는 다음을 제어합니다.

- Primitives
- BillboardText
- Grid
- Gizmo
- Bounding Volume

### 9. 선택, 피킹, 아웃라인

- 마우스 클릭 기반 선택
- 메시 레이캐스트 기반 피킹
- 선택 오브젝트 스텐실 마스크 렌더링
- 후처리 아웃라인 표시
- 선택 상태에 맞춰 기즈모 자동 동기화

### 10. 씬 저장 및 복원

씬은 `.Scene` 확장자의 JSON 파일로 저장됩니다.

저장되는 정보:

- 월드 타입
- 액터/컴포넌트 계층
- 컴포넌트 프로퍼티
- StaticMesh 경로
- Perspective 카메라 상태

즉, 편집 결과를 다시 열 수 있는 최소 단위의 씬 직렬화가 구현되어 있습니다.

### 11. 리소스 관리와 캐시

리소스 매니저는 에셋 검색, 로딩, 캐시를 담당합니다.

- 메시 경로 스캔
- 머티리얼 파일 스캔
- 텍스처 스캔
- 로드된 StaticMesh 캐시
- 머티리얼 레지스트리 유지

StaticMesh 로드는 다음 순서로 진행됩니다.

1. 메모리 캐시 확인
2. 대응하는 바이너리 파일 유효성 검사
3. 유효하면 바이너리 로드
4. 실패하면 OBJ 파싱
5. OBJ 파싱 성공 시 바이너리 저장

### 12. OBJ 바이너리 베이킹

텍스트 OBJ를 매번 다시 파싱하지 않도록 바이너리 캐시를 생성합니다.

- 전용 헤더 사용
  - Magic Number
  - Version
  - Vertex/Index/Section/Slot count
  - 원본 파일 수정 시간
- 원본 OBJ 수정 시간이 바뀌면 바이너리 무효화
- 재로드 시 바이너리 우선 사용

캐시 파일은 `Asset/Mesh/Bin` 아래에 `.bin` 형태로 저장됩니다.

### 13. ObjViewer 모드

프로젝트에는 별도 `ObjViewer` 빌드 구성이 포함되어 있습니다.

- `IS_OBJ_VIEWER=1` 전처리 매크로 사용
- 전용 엔진 클래스 `UObjViewerEngine`
- 파일 다이얼로그로 `.obj` 선택 로드
- 미리보기용 `UStaticMeshComponent` 사용
- 카메라 리셋 기능
- 우측 패널에 기본 메시 통계 표시

ObjViewer는 "에디터 전체 기능"이 아니라, 모델 확인에 집중한 별도 실행 모드입니다.

## 엔진 구조

### 객체 시스템

- `UObject` 기반 공통 객체 시스템
- `DECLARE_CLASS`, `DEFINE_CLASS` 기반 커스텀 RTTI
- `IsA<T>()`, `Cast<T>()` 지원
- `UObjectManager`를 통한 생성/파괴 관리
- UUID와 내부 인덱스 보유

### 이름 시스템

- `FName` 이름 풀 사용
- Display 이름과 Comparison 이름을 분리 저장
- 비교 시 소문자 기반 인덱스 비교

즉, 표시용 문자열과 비교용 문자열을 나눠 관리하는 방식입니다.

### 객체 순회

- `TObjectIterator<T>` 구현
- 글로벌 UObject 배열을 순회하며 타입이 맞는 객체만 반환

## 폴더 개요

```text
W4_Jungle_Team2/
├─ W4_Jungle_Team2/
│  ├─ Source/
│  │  ├─ Engine/
│  │  │  ├─ Asset/
│  │  │  ├─ Component/
│  │  │  ├─ Core/
│  │  │  ├─ GameFramework/
│  │  │  ├─ Math/
│  │  │  ├─ Object/
│  │  │  ├─ Render/
│  │  │  ├─ Runtime/
│  │  │  ├─ Serialization/
│  │  │  └─ Slate/
│  │  ├─ Editor/
│  │  └─ Misc/ObjViewer/
│  ├─ Asset/
│  ├─ Shaders/
│  ├─ Settings/
│  └─ Bin/
└─ W4_Jungle_Team2.sln
```

## 실행과 빌드

### 권장 환경

- Windows
- Visual Studio 2022
- DirectX 11 지원 환경

### 솔루션

- 솔루션 파일: `W4_Jungle_Team2.sln`

### 주요 빌드 구성

- `Debug | x64`
- `Release | x64`
- `ObjViewer | x64`

`ObjViewer` 구성은 전용 모델 뷰어 모드입니다.

## 문서 범위에서 제외한 것

아래 항목은 발표자료에 언급되더라도, 이 문서에서는 과장하지 않기 위해 표현을 낮추거나 일반화했습니다.

- 범용 게임 엔진 수준의 완성도
- 범용 머티리얼 에디터
- 모든 OBJ/MTL 조합에 대한 완전 호환
- 고급 메시 최적화 기능 전반

현재 리포지토리 기준으로는 "에디터 기능을 갖춘 DirectX 11 기반 학습용 3D 엔진/도구 프로젝트"로 보는 것이 가장 정확합니다.
