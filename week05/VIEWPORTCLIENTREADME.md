# ViewportClient 구조 정리

## 목적

이 문서는 현재 프로젝트에서 `Renderer`, `Core`, `ViewportClient`, `EditorUI`가 어떤 역할로 나뉘는지 정리하기 위한 문서다.

핵심 목적은 다음과 같다.

- `Renderer`는 공용 GPU 백엔드로 유지한다.
- 에디터와 게임의 렌더링 차이는 `ViewportClient` 정책으로 분리한다.
- 나중에 `PreviewViewportClient` 같은 별도 뷰포트 정책을 쉽게 추가할 수 있게 한다.

## 한 줄 요약

`Renderer`는 "그리는 기계"이고, `ViewportClient`는 "이번 창을 게임처럼 그릴지 에디터처럼 그릴지 결정하는 정책 객체"다.

## 주요 객체 역할

### `CRenderer`

파일:

- `Engine/Source/Renderer/Renderer.h`
- `Engine/Source/Renderer/Renderer.cpp`

역할:

- D3D11 디바이스, 스왑체인, RTV/DSV, 셰이더, 상수버퍼를 관리한다.
- 렌더 커맨드를 GPU에 제출하고 실제 draw call을 실행한다.
- ImGui, post-render 같은 추가 기능은 callback으로만 받는다.

중요한 점:

- `Renderer`는 이제 직접 "에디터 렌더러"나 "게임 렌더러"가 아니다.
- 공용 렌더링 백엔드 역할만 한다.

### `CCore`

파일:

- `Engine/Source/Core/Core.h`
- `Engine/Source/Core/Core.cpp`

역할:

- 프레임 루프 중 실제 게임/에디터 실행 흐름을 관리한다.
- `Renderer`, `InputManager`, `SceneContext`들을 소유한다.
- 매 프레임 `Tick -> Input -> GameLogic -> Render` 순서로 진행한다.
- 현재 설치된 `ViewportClient`를 통해 렌더링 정책을 위임한다.

중요한 점:

- `Renderer`의 직접 소유자는 `CCore`다.
- `CCore`는 "언제 렌더할지"를 결정하고, `ViewportClient`는 "어떻게 렌더할지"를 결정한다.

### `IViewportClient`

파일:

- `Engine/Source/Core/ViewportClient.h`
- `Engine/Source/Core/ViewportClient.cpp`

역할:

- 뷰포트별 렌더링 정책 인터페이스다.
- 현재 활성 scene을 무엇으로 볼지 결정한다.
- 어떤 render command를 만들지 결정한다.
- 필요하면 renderer에 GUI/post-render 같은 callback을 붙인다.

현재 함수 의미:

- `Attach(Core, Renderer)`
  - 해당 뷰포트 정책이 활성화될 때 호출된다.
- `Detach(Core, Renderer)`
  - 다른 뷰포트 정책으로 전환되기 전에 호출된다.
- `ResolveScene(Core)`
  - 이번 프레임에 어떤 scene을 렌더할지 결정한다.
- `BuildRenderCommands(Core, Scene, Frustum, OutQueue)`
  - scene으로부터 render command를 어떻게 수집할지 결정한다.

### `CGameViewportClient`

파일:

- `Engine/Source/Core/ViewportClient.cpp`

역할:

- 게임 클라이언트 기본 뷰포트 정책이다.
- 에디터용 callback을 renderer에서 제거한다.
- 순수 게임 렌더링 상태를 만든다.

현재 동작:

- `Attach()` 시 `Renderer->ClearViewportCallbacks()` 호출
- `Detach()` 시에도 callback 정리

즉:

- ImGui 없음
- outline 없음
- 축 표시 없음
- 순수 scene 렌더링만 수행

### `CEditorViewportClient`

파일:

- `Editor/Source/UI/EditorViewportClient.h`
- `Editor/Source/UI/EditorViewportClient.cpp`

역할:

- 에디터 전용 뷰포트 정책이다.
- `EditorUI`를 renderer에 연결한다.
- 에디터 창에서 필요한 ImGui, outline, world axis 같은 기능을 켠다.

현재 동작:

- `Attach()`에서
  - `EditorUI.Initialize(Core)`
  - `EditorUI.SetupWindow(MainWindow)`
  - `EditorUI.AttachToRenderer(Renderer)`
- `Detach()`에서
  - `EditorUI.DetachFromRenderer(Renderer)`

### `CEditorUI`

파일:

- `Editor/Source/UI/EditorUI.h`
- `Editor/Source/UI/EditorUI.cpp`

역할:

- 에디터 패널과 ImGui 렌더링을 담당한다.
- 선택된 actor의 outline, 월드 축 라인, property/control panel UI를 담당한다.
- 윈도우 메시지 필터를 등록한다.

현재 구조에서 중요한 점:

- `Initialize()`는 내부 상태와 property callback만 세팅한다.
- `AttachToRenderer()`가 실제로 renderer에 ImGui와 post-render 동작을 설치한다.
- `DetachFromRenderer()`가 renderer에서 editor 전용 기능을 제거한다.

즉:

- `EditorUI`는 항상 renderer에 붙어 있는 것이 아니다.
- `EditorViewportClient`가 활성일 때만 붙는다.

## 실제 시작 순서

### 게임 클라이언트 시작

1. `FEngine::Initialize()`
2. 윈도우 생성
3. `CCore` 생성
4. `CCore::Initialize()` 안에서 `Renderer`, `InputManager`, `Scene` 초기화
5. `FEngine::CreateViewportClient()` 호출
6. 기본 구현으로 `CGameViewportClient` 생성
7. `Core->SetViewportClient(GameViewportClient)`
8. `GameViewportClient::Attach()` 호출
9. renderer는 공용 렌더링 상태만 유지

### 에디터 시작

1. `FEditorEngine::Initialize()`
2. 내부적으로 `FEngine::Initialize()` 수행
3. `CCore` 생성 및 초기화
4. `FEditorEngine::CreateViewportClient()` 호출
5. `CEditorViewportClient` 생성
6. `Core->SetViewportClient(EditorViewportClient)`
7. `EditorViewportClient::Attach()` 호출
8. `EditorUI`가 renderer에 연결됨
9. ImGui / outline / world axis / editor panel 렌더링 활성화

## 프레임 흐름

현재 프레임 흐름은 대략 다음과 같다.

1. `FEngine::Run()`
2. 메시지 펌프 실행
3. `Core->Tick()`
4. `InputManager.Tick()`
5. 카메라 입력 처리
6. `Physics()`
7. `GameLogic()`
8. `Core->Render()`

### `Core->Render()` 안에서 일어나는 일

1. `ViewportClient->ResolveScene(Core)` 호출
2. 이번 프레임에 렌더할 scene 결정
3. `Renderer->BeginFrame()`
4. 카메라에서 View / Projection 계산
5. Frustum 계산
6. `ViewportClient->BuildRenderCommands(...)` 호출
7. render command queue 작성
8. `Renderer->SubmitCommands(...)`
9. `Renderer->ExecuteCommands()`
10. `Renderer->EndFrame()`

즉:

- `CCore`는 프레임 진행자
- `ViewportClient`는 렌더링 정책
- `Renderer`는 실제 실행기

## 게임과 에디터가 실제로 어떻게 달라지는가

### 게임

- `CGameViewportClient` 사용
- renderer callback 없음
- scene만 렌더링
- GUI, outline, axis 없음

### 에디터

- `CEditorViewportClient` 사용
- renderer에 ImGui callback 등록
- renderer에 post-render callback 등록
- scene 렌더 후 outline, axis line 등을 추가 렌더링
- 윈도우 메시지 필터를 통해 ImGui 입력과 picking 활성화

## 이 구조의 장점

### 1. `Renderer`를 공용으로 유지할 수 있다

에디터/게임이 서로 다른 렌더러 클래스를 갖지 않아도 된다.

### 2. 정책 분리가 쉬워진다

에디터 전용 기능이 `Renderer` 본체에 직접 섞이지 않는다.

### 3. 확장이 쉬워진다

나중에 아래처럼 별도 정책을 추가하기 쉽다.

- `CPreviewViewportClient`
- `CMaterialPreviewViewportClient`
- `CStaticMeshPreviewViewportClient`

현재는 `CPreviewViewportClient`가 추가되어 있고, active scene type이 `Preview`일 때 에디터 엔진이 이 정책으로 자동 전환한다.
이 단계에서는 preview scene을 메인 렌더 경로로 전환해서 보여주고, 별도 preview 패널 동시 렌더링은 아직 하지 않는다.

### 4. UE 스타일 구조에 가까워진다

UE도 공통 렌더링 백엔드를 두고, viewport별 client가 정책을 다르게 가진다.

## 현재 한계

현재는 1차 분리까지 되어 있다.

분리된 항목:

- ImGui 초기화와 렌더링
- editor post-render
- game/editor callback 생명주기

아직 추가로 분리할 수 있는 항목:

- editor camera 입력
- picking 정책
- preview 전용 render command 생성
- editor/game별 show flag
- scene save/load 시 GPU device 의존성 제거

## 앞으로 자연스러운 다음 단계

### 1. `PreviewViewportClient` 추가

`PreviewScene`를 별도 viewport 정책으로 다룰 수 있다.

예:

- 머티리얼 프리뷰
- 메시 프리뷰
- gizmo 프리뷰

### 2. 입력 정책도 `ViewportClient` 쪽으로 이동

현재 일부 입력은 아직 `EditorUI`와 `CCore`에 남아 있다.

장기적으로는:

- 게임 입력
- 에디터 카메라 입력
- ImGui 입력
- preview 입력

도 viewport client 단위로 정리하는 것이 좋다.

### 3. scene asset loading의 GPU 의존성 제거

현재 `LoadSceneFromFile(..., Device)` 같은 경로는 scene이 GPU device를 직접 아는 구조다.

장기적으로는:

- scene은 데이터만 다루고
- material/resource 생성은 renderer나 resource service가 맡는 편이 더 좋다.

## 최종 요약

현재 구조는 다음처럼 이해하면 된다.

- `FEngine`
  - 앱과 창을 올리고 `Core`와 `ViewportClient`를 연결하는 상위 부트스트랩
- `CCore`
  - 프레임 루프와 scene context를 관리하는 실행 관리자
- `IViewportClient`
  - 이번 창의 렌더링 성격을 결정하는 정책 객체
- `CRenderer`
  - GPU에 실제 draw call을 내리는 공용 렌더링 백엔드

즉:

- 게임은 `CGameViewportClient + Renderer`
- 에디터는 `CEditorViewportClient + Renderer`

구조로 동작한다.
