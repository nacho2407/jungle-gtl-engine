# Merge Resolution Log: main → feature/engine/rotator

**날짜**: 2026-04-01
**병합 방향**: main → feature/engine/rotator

## 충돌 목록 및 해결 방법

### 1. CraftonEngine/Source/Engine/Core/PropertyTypes.h (modify/delete)
- **원인**: main에서 CraftonEngine → KraftonEngine 디렉토리 리네임으로 삭제됨, rotator에서 `Rotator` enum 추가
- **해결**: 구 경로 파일 삭제(`git rm`). KraftonEngine 경로의 PropertyTypes.h에 `Rotator` enum 항목 추가하여 양쪽 변경 병합

### 2. KraftonEngine/Settings/Editor.ini (content conflict)
- **원인**: 양쪽에서 에디터 설정값 변경
- **해결**: main 버전 채택 (`git checkout --theirs`). 에디터 설정은 최신 main 기준이 적합

### 3. KraftonEngine/Source/Engine/Component/GizmoComponent.h (content conflict)
- **원인**: main에서 EGizmoMode를 클래스 밖으로 이동 + `ViewTypes.h` include 추가, rotator에서 `Rotator.h` include + `FRotator` 시그니처 변경
- **해결**: 양쪽 include 모두 유지, EGizmoMode 외부 정의(main) + FRotator 기반 시그니처(rotator) 병합

### 4. KraftonEngine/Source/Engine/GameFramework/AActor.h (content conflict)
- **원인**: main에서 inline 함수를 .cpp로 이동 + Transform 헬퍼 추가, rotator에서 Rotation을 FRotator로 변경
- **해결**: main의 선언-only 패턴 유지하면서 FRotator 반환/매개변수 시그니처 적용. AActor.cpp에 FRotator 구현 추가

### 5. KraftonEngine/Source/Engine/Math/Matrix.cpp (content conflict)
- **원인**: main에서 `Utils.h` → `MathUtils.h` 리네임, rotator에서 `Quat.h`/`Rotator.h` include + FQuat/FRotator 관련 함수 추가
- **해결**: `MathUtils.h`(main) + `Quat.h`/`Rotator.h`(rotator) include 모두 유지, FQuat 기반 행렬 함수 보존

### 6. Quat.cpp/h, Rotator.cpp/h (file location conflict)
- **원인**: rotator 브랜치가 구 경로(`CraftonEngine/`)에서 파일 생성, main에서 디렉토리가 `KraftonEngine/`으로 리네임됨
- **해결**: `KraftonEngine/Source/Engine/Math/` 경로에 파일 배치. include 경로를 `Utils.h` → `MathUtils.h`로 업데이트

### 7. KraftonEngine/imgui.ini (content conflict)
- **원인**: 양쪽에서 ImGui 레이아웃 변경
- **해결**: main 버전 채택 (`git checkout --theirs`). ImGui 레이아웃은 최신 main 기준

## 빌드 후 추가 수정

### GizmoComponent.cpp:513 — FRotator → FVector 변환
- **증상**: `FMatrix::MakeRotationEuler(GetRelativeRotation())` 에서 `GetRelativeRotation()`이 `FRotator`를 반환하는데 `MakeRotationEuler`는 `FVector`를 요구
- **해결**: `.ToVector()` 호출 추가 → `FMatrix::MakeRotationEuler(GetRelativeRotation().ToVector())`
