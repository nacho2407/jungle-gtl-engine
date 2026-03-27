# Gizmo 구현 정리

## 목적

이 문서는 현재 프로젝트의 `Translation`, `Rotation`, `Scale` gizmo가 어떤 구조로 구현되어 있고, 실제로 어떤 수학과 입력 흐름으로 동작하는지 정리한 기술 문서다.

대상 독자는 다음과 같다.

- 에디터 입력과 렌더링 경로를 수정하려는 사람
- gizmo 피킹이나 drag 감각을 보정하려는 사람
- `Local / World`, `screen handle`, `negative scale` 같은 정책이 어디에 구현되어 있는지 찾아야 하는 사람

## 전체 구조

현재 gizmo는 크게 4계층으로 나뉜다.

1. `EditorViewportClient`
   - 뷰포트 입력을 받는다.
   - 모드 전환과 마우스 메시지를 `CGizmo`로 전달한다.
2. `CGizmo`
   - 현재 gizmo 상태를 관리한다.
   - 렌더링용 `RenderCommand`를 만든다.
   - 피킹, hover, drag 시작/업데이트/종료를 처리한다.
3. `CPrimitiveGizmo`
   - gizmo sub-mesh를 `FMeshData`로 변환한다.
4. `UnrealEditorStyledGizmo`
   - 실제 vertex/index를 생성하는 메쉬 생성기다.

즉, 역할을 한 줄로 정리하면 다음과 같다.

- `EditorViewportClient` = 입력 라우터
- `CGizmo` = 상태 머신 + 수학 처리
- `CPrimitiveGizmo` = 엔진 메쉬 포맷 어댑터
- `UnrealEditorStyledGizmo` = 실제 모양 생성기

## 관련 파일

### 에디터 쪽

- `Editor/Source/UI/EditorViewportClient.cpp`
- `Editor/Source/Gizmo/Gizmo.h`
- `Editor/Source/Gizmo/Gizmo.cpp`

### 엔진 쪽

- `Engine/Source/Primitive/PrimitiveGizmo.h`
- `Engine/Source/Primitive/PrimitiveGizmo.cpp`
- `Engine/Source/Primitive/UnrealEditorStyledGizmo.h`
- `Engine/Source/Primitive/UnrealEditorStyledGizmo.cpp`

## 입력 흐름

입력 진입점은 `CEditorViewportClient::HandleMessage()`다.

현재 단축키는 다음과 같다.

- `W` : Translation
- `E` : Rotation
- `R` : Scale
- `L` : `World / Local` 토글

마우스 메시지 흐름은 다음 순서다.

1. 메인 윈도우 좌표를 뷰포트 내부 좌표로 변환한다.
2. `WM_LBUTTONDOWN`이면 먼저 gizmo hit test를 한다.
3. gizmo를 잡지 못했을 때만 actor picking을 한다.
4. `WM_MOUSEMOVE`에서 drag 중이면 `UpdateDrag()`, drag 중이 아니면 `UpdateHover()`를 호출한다.
5. `WM_LBUTTONUP`이면 drag를 종료하고 hover를 다시 계산한다.

중요한 점은 **gizmo가 actor picking보다 항상 우선**이라는 점이다.

## `CGizmo`가 관리하는 상태

`CGizmo`는 아래 상태를 들고 있다.

- `Mode`
  - `Location`
  - `Rotation`
  - `Scale`
- `CoordinateSpace`
  - `World`
  - `Local`
- `ActiveAxis`
  - 현재 drag 중인 축 또는 핸들
- `HoveredAxis`
  - 현재 마우스가 올라간 축 또는 핸들
- drag 시작 시점 캐시
  - 시작 actor 위치
  - 시작 actor 회전
  - 시작 actor scale
  - 시작 교점
  - drag plane normal
  - 시작 마우스 좌표

즉 `CGizmo`는 “그 순간 gizmo가 어떤 상태인지”를 메모리로 유지하는 상태 머신이다.

## 렌더링 경로

렌더링 entry point는 `CGizmo::BuildRenderCommands()`다.

이 함수는 다음 순서로 동작한다.

1. 선택된 actor와 camera를 확인한다.
2. actor 월드 위치를 기준으로 gizmo 중심을 잡는다.
3. 카메라 거리와 FOV를 이용해 gizmo 화면 크기를 계산한다.
4. 현재 모드에 따라 적절한 sub-mesh를 가져온다.
5. `FRenderCommand`를 만들어 queue에 넣는다.
6. hover 또는 active 상태면 노란색 highlight mesh를 추가로 넣는다.

렌더 상태는 overlay pass다.

- `bOverlay = true`
- `bDisableDepthTest = true`
- `bDisableDepthWrite = true`
- `bDisableCulling = true`

즉 gizmo는 일반 오브젝트 위에 항상 보이도록 그려진다.

## gizmo 스케일 계산

기본 크기 계산은 `CGizmo::ComputeGizmoScale()`에서 한다.

개념은 다음과 같다.

1. `Distance = |WorldPosition - CameraPosition|`
2. `VisibleHeight = 2 * Distance * tan(FOV / 2)`
3. `DesiredAxisLength = VisibleHeight * GizmoViewportHeightRatio`
4. 이 길이를 gizmo의 기준 길이 상수로 나누어 월드 스케일을 얻는다.

즉 카메라가 멀어지면 gizmo도 같이 커져서 화면상 크기가 너무 작아지지 않게 만든다.

현재 기준 길이 상수는 모드별로 다르다.

- Translation / Rotation : `TranslationAxisLengthUnits`
- Scale : `ScaleAxisLengthUnits`

추가로 `Scale`은 렌더링에서 `GetRenderGizmoScale()`을 통해 절반 크기로 한 번 더 줄인다.

현재는 **render, hit test, scale drag sensitivity가 모두 같은 축소된 scale 기준**을 사용한다.

## 피킹 구조

피킹 entry point는 `CGizmo::HitTestAxis()`다.

작동 방식은 다음과 같다.

1. 뷰포트 마우스 좌표를 `Picker::ScreenToRay()`로 월드 ray로 바꾼다.
2. 현재 모드에 필요한 sub-mesh를 확보한다.
3. gizmo 월드 행렬을 만든다.
4. 각 sub-mesh의 삼각형을 순회한다.
5. ray와 삼각형의 교차를 검사한다.
6. 가장 가까운 교차 거리를 가진 핸들을 선택한다.

즉 현재 gizmo 피킹은 “대략적인 선분 거리 판정”이 아니라 **실제 sub-mesh 삼각형 기반 피킹**이다.

교차 함수는 `RayTriangleIntersectTwoSided()`이며, 현재는 양면 검사다.

모드별 피킹 대상은 다음과 같다.

### Translation

- 축 3개
- 평면 3개
- 중앙 screen handle

### Rotation

- 축 링 3개
- screen ring

### Scale

- 축 3개
- 평면 3개
- 중앙 uniform scale cube

## Translation drag 수학

Translation은 `BeginTranslationDrag()`와 `UpdateDrag()`에서 처리한다.

### 축 이동

축 이동은 다음 순서다.

1. 축 방향 벡터를 구한다.
2. 카메라 기준으로 그 축을 포함하는 drag plane을 만든다.
3. drag 시작 시 ray-plane 교점을 구한다.
4. 현재 프레임에서도 ray-plane 교점을 구한다.
5. 교점 벡터를 축 방향으로 내적해서 이동량을 얻는다.

수식 개념은 다음과 같다.

- `axisDistance = dot(intersection - gizmoOrigin, axis)`
- `delta = currentAxisDistance - startAxisDistance`
- `newLocation = startLocation + axis * delta`

### 평면 이동

평면 이동은 더 단순하다.

1. `XY / XZ / YZ`에 해당하는 plane normal을 만든다.
2. 시작 교점과 현재 교점을 구한다.
3. 두 점 차이를 그대로 위치 오프셋으로 사용한다.

### 중앙 screen handle

중앙 핸들은 카메라 정면 plane에서 자유 이동한다.

즉 “화면을 따라 이동하는 느낌”을 주기 위해 `planeNormal = cameraForward`를 사용한다.

## Rotation drag 수학

Rotation은 `BeginRotationDrag()`와 `UpdateDrag()`에서 처리한다.

기본 아이디어는 **회전축에 수직인 평면에서 시작 벡터와 현재 벡터의 signed angle을 계산하는 방식**이다.

### 시작 단계

1. 선택 축을 회전축으로 잡는다.
2. 그 축을 normal로 하는 평면을 만든다.
3. 마우스 ray와 평면의 교점을 구한다.
4. `startVector = normalize(intersection - gizmoOrigin)`를 저장한다.

### 업데이트 단계

1. 현재 ray와 같은 평면의 교점을 구한다.
2. `currentVector = normalize(intersection - gizmoOrigin)`를 구한다.
3. 시작 벡터와 현재 벡터의 signed angle을 구한다.

각도 계산은 다음 형태다.

- `cross = cross(startVector, currentVector)`
- `angle = atan2(dot(cross, axis), dot(startVector, currentVector))`

이 각도를 `FQuat(axis, angle)`로 바꿔 시작 회전에 곱한다.

즉 회전은 “마우스 delta를 직접 각도로 쓰는 방식”이 아니라 **평면 위의 방향 벡터 두 개의 각도**로 계산한다.

### screen rotation

`Screen` 회전은 일반 축 대신 카메라 forward를 회전축으로 사용한다.

즉 화면 정면 기준으로 도는 회전이다.

## Scale drag 수학

Scale은 `BeginScaleDrag()`와 `UpdateDrag()`에서 처리한다.

현재 버전은 1차 구현이며, 개념은 다음과 같다.

### 축 scale

1. 선택 축을 기준으로 drag plane을 만든다.
2. 시작 교점과 현재 교점의 축 방향 거리 차이를 구한다.
3. 그 값을 scale delta로 변환한다.
4. 해당 축 성분만 바꾼다.

즉:

- `dot(intersection - gizmoOrigin, axis)` 값 차이로 scale을 만든다.

### 평면 scale

평면 scale은 평면 위 이동량을 각 축으로 다시 투영해서 사용한다.

예를 들어 `XY`면:

- `deltaX = dot(offset, xAxis)`
- `deltaY = dot(offset, yAxis)`

처럼 계산하고 `Scale.X`, `Scale.Y`만 바꾼다.

### uniform scale

중앙 큐브는 픽셀 이동량을 단순한 scalar 값으로 바꿔 세 축에 동일하게 적용한다.

현재는:

- `pixelDelta = (ScreenX - StartX) - (ScreenY - StartY)`

를 이용해 하나의 uniform delta를 만들고 `X/Y/Z`에 같이 더한다.

### clamp

scale은 너무 0에 가까워지거나 너무 커지지 않도록 clamp한다.

- 최소 절댓값: `MinScaleMagnitude`
- 최대 절댓값: `MaxScaleMagnitude`

## Local / World 정책

`CoordinateSpace`는 gizmo 방향 계산에만 영향을 준다.

현재 정책은 다음과 같다.

### Translation

- `World`면 월드 축 사용
- `Local`이면 actor 회전을 적용한 local 축 사용

### Rotation

- `World`면 월드 축 사용
- `Local`이면 actor 회전을 적용한 local 축 사용
- `Screen` 링은 항상 화면 기준

### Scale

Scale은 현재 **Local 고정**이다.

즉 `L` 토글 상태와 무관하게 scale gizmo 방향은 항상 actor의 local 회전을 따른다.

이건 UE 쪽 사용감에 맞춘 정책이다.

## Negative scale 반전 방지

일반적으로 world transform에서 rotation을 추출하면 negative scale이 포함된 경우 축 부호가 뒤집혀 gizmo도 반전될 수 있다.

현재 scale 모드에서는 이 문제를 피하기 위해 `GetComponentWorldRotationIgnoringScale()`를 사용한다.

이 함수는 다음 방식으로 동작한다.

1. 현재 component의 `RelativeTransform().GetRotation()`만 읽는다.
2. 부모가 있으면 부모 회전도 같은 방식으로 재귀적으로 읽는다.
3. 부모-자식 회전 쿼터니언만 곱해 world rotation을 만든다.

즉:

- scale 부호는 무시
- 회전 체인만 유지

하는 방식이다.

그래서 대상 오브젝트 scale이 `(-1, 1, 1)`이어도 scale gizmo는 mirror되지 않는다.

## Rotation mesh의 view-dependent 처리

Rotation gizmo는 항상 full ring을 그리지 않는다.

기본 상태에서는 카메라 방향을 보고 축별로 보이는 arc만 선택한다.
활성 축을 drag 중이면:

- 비활성 축은 숨기고
- 활성 축만 full ring 또는 active arc 형태로 다시 생성한다

이 로직은 `RotationDesc`를 통해 `UnrealEditorStyledGizmo.cpp`까지 전달된다.

즉 rotation gizmo는 정적 mesh가 아니라 **카메라 상태와 drag 상태에 따라 다시 생성되는 동적 mesh**다.

## 하이라이트 정책

하이라이트는 기본적으로 `HoveredAxis` 또는 `ActiveAxis`에 따라 별도 노란색 sub-mesh를 하나 더 그리는 방식이다.

### Translation

- 축 hover -> 해당 축만 노란색
- 평면 hover -> 해당 평면 + 관련 축 2개도 노란색
- screen handle hover -> 중앙 handle만 노란색

### Rotation

- hover / active 축만 노란색
- screen ring도 별도 하이라이트 가능

### Scale

- 축 / 평면 / 중앙 큐브 각각 개별 하이라이트

즉 base gizmo mesh를 바꾸는 게 아니라 **노란 highlight mesh를 추가로 overlay 렌더**하는 구조다.

## 현재 제약

현재 구현에 남아 있는 기술적 제약은 다음과 같다.

- undo / redo 연계 없음
- translate / rotate / scale snap 없음
- scale drag 감각은 1차 버전이라 더 다듬을 여지 있음
- rotation drag 감각은 UE와 완전히 동일하지 않음
- gizmo 상태를 툴바나 패널에 별도로 표시하지 않음
- 일부 상수는 translation / scale이 공유하고 있어 분리 여지가 있음

## 요약

현재 gizmo는 다음 구조로 이해하면 된다.

1. `EditorViewportClient`가 입력을 받는다.
2. `CGizmo`가 상태, 피킹, drag 수학을 처리한다.
3. `CPrimitiveGizmo`가 필요한 sub-mesh를 `FMeshData`로 만든다.
4. `Renderer`는 overlay pass로 이를 그린다.

즉 현재 구현은 **입력, 수학, 메쉬 생성, 렌더링을 분리한 구조**이며, 모드별 차이는 대부분 `CGizmo`의 상태와 `UnrealEditorStyledGizmo`의 메쉬 생성 정책으로 결정된다.
