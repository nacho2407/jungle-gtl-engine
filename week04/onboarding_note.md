# KRAFTON Jungle game tech Lab 4주차 프로젝트 온보딩 노트

## 1. 프로젝트 개요

- 프로젝트 이름: 게임 엔진 제작 실습

- 프로젝트 목적: Unreal Engine과 유사한 게임 엔진 개발 실습

- 이번에 내가 맡은 작업

    - Object Iterator 구현

    - Overlay Stat 렌더링 구현

    - OBJ 뷰어 구현


## 2. 실행 정보

- 빌드 방법: Visual Studio(msbuild)를 통한 빌드

- 실행 방법: Visual Studio를 통한 실행 / 빌드 파일(`*.exe`) 실행

- 주요 설정 파일

    - `Editor/imgui.ini`: ImGui 패널 관련 설정

    - `Editor/Saved/Config/editor.ini`: 뷰포트(카메라 관련) 설정

- 주의할 런타임 리소스 경로

    - `Editor/Content/*`: 폰트, 씬 저장 정보, 셰이더, 텍스처 아틀라스


## 3. 디렉터리 구조

### 3.1. Editor

- `/Editor/Source/`: 에디터 프로그램의 진입점(`main.cpp`) 포함

    - `Camera/`: 에디터 뷰포트 카메라 상태와 시점 계산 관련 소스

    - `Chrome/`: 에디터 상단 메뉴바, 커스텀 타이틀바, 창 제어 버튼 등 Editor Chrome UI 관련 소스

    - `Content/`: Content Browser가 사용할 콘텐츠 파일 시스템 snapshot 및 인덱스 모델 관련 소스

        - snapshot 방식을 사용하여 매번 파일 시스템을 뒤져야하는 비용은 줄였지만 파일 변경 내용이 실시간 반영되지는 않으며, 대신 refresh 버튼을 두어 새로고침 할 수 있도록 하였음(디렉토리 탐색기에서는 일반적인 구조임. Windows 탐색기도 그래). 물론 껐다 키면 변경 사항 반영되어 있음

    - `Editor/`: 에디터 애플리케이션 전체 수명주기, 공용 context, 설정, 로그 버퍼 등 최상위 에디터 오케스트레이션 소스

    - `Gizmo/`: 에디터 gizmo 상태 표현과 gizmo 렌더 전달용 데이터 변환 관련 소스

        - 기즈모 상태 데이터나 기즈모를 그리기위한 데이터는 정작 Render 쪽 디렉토리에 있고 `Gizmo` 디렉토리에는 변환용 인라인 함수만 있는데 이건 아직 구조가 좀 미성숙한 부분으로 봐야할 듯

    - `Input/`: 에디터 입력을 전역 명령(단축키등), 뷰포트 이동, 객체(액터) 선택, gizmo 조작 등의 문맥으로 해석하는 입력 컨텍스트 소스

    - `Launch/`: 에디터 프로그램의 실행 루프, 초기화, 종료, 프레임 반복 등 launch 계층 소스

    - `Menu/`: Editor Chrome UI에 나오는 메뉴 명령 정의, 메뉴 등록, 메뉴 트리 구성 관련 소스

    - `Panel/`: Console, Content Browser, Outliner, Properties 등 ImGui 기반 에디터 패널들과 패널 관리 소스

    - `ThirdParty/`: 외부 라이브러리 소스

    - `Viewport/`: 에디터 뷰포트 편집 기능 전반을 담당하는 소스

        - `Gizmo/`: 뷰포트 내 gizmo 조작 로직 관련 소스

        - `Global/`: 에디터 전역 명령이나 씬 단위 동작을 연결하는 상위 컨트롤러 소스

            - `FEditorGlobalContext`에서 단축키 관련 정보를 해석하여 `FEditorGlobalController`에서 처리를 수행

        - `Interaction/`: 뷰포트 자체 상호작용(마우스 캡처 / 해제, 커서 락, 포커스 등) 상태 추적 관련 소스

        - `Navigation/`: 뷰포트 카메라 이동 / 회전 / orbit / pan 등 네비게이션 관련 소스

        - `RenderSetting/`: 뷰포트 표시 옵션, view mode, show flag 구성 관련 소스

        - `Selection/`: 뷰포트 오브젝트 피킹 및 선택 상태 동기화 관련 소스


### 3.2. Engine

- `/Engine/Source/`

    - `ApplicationCore/`

        - `GenericPlatform/`: 플랫폼 공통 인터페이스나 플랫폼 추상화 기반 소스

        - `Input/`: 입력 이벤트, 입력 상태, 입력 시스템, 입력 라우터같은 엔진 공용 입력 처리 소스

        - `Windows/`: Win32 전용 application / window 구현 소스

    - `Asset/`: 텍스처, 폰트, SubUV Atlas 등 에셋 로딩 / 등록 / 관리 관련 소스

    - `Core/`: 엔진 전반에서 공통으로 사용하는 저수준 기반 라이브러리 소스

        - PCH 포함

        - `Containers/`: 배열, 맵, 문자열 등 기본 컨테이너 / 자료구조 소스

        - `Geometry/`: 기하 프리미티브, 교차 판정, 변환 등 기하 관련 소스

            - `Primitives/`

        - `HAL/`: 플랫폼 타입, 시간, 메모리, 플랫폼 유틸리티 등 하드웨어 / 플랫폼 추상화 소스

        - `Logging/`: 로그 매크로와 로그 출력 장치 인터페이스 관련 소스

        - `Math/`: 벡터, 행렬, 쿼터니언, 회전, 색상 등 수학 타입 / 연산 소스

        - `Misc/`: 이름 시스템 등 범용 유틸리티 소스

            - enum을 비트 플래그 집합처럼 다루기 위한 `BitMaskEnum` 포함

    - `CoreUObject/`: 엔진의 기본 오브젝트 모델, 팩토리 등 UObject 성격의 기초 객체 시스템 소스

    - `Engine/`: 씬, 액터, 컴포넌트, 뷰포트 등 게임 런타임 중심의 상위 엔진 계층 소스

        - `Component/`

            - `Core/`: `SceneComponent`, `PrimitiveComponent` 등 기본 컴포넌트 추상화 소스

            - `Mesh/`: 기본 메시 프리미티브 렌더링용 컴포넌트 소스

            - `Sprite/`: 스프라이트, SubUV, 아틀라스 기반 2D 렌더링 컴포넌트 소스

            - `Text/`: 텍스트 / UUID 등 텍스트성 렌더링 및 표시용 컴포넌트 소스

        - `Game/`: 각종 Actor 클래스와 게임 오브젝트 단위 소스

        - `ViewPort/`: 엔진 공용 viewport 및 viewport client 추상화 소스

    - `Launch/`: 엔진 루프 인터페이스 등 실행 진입 / 수명주기 기반 소스

    - `Renderer/`: 렌더러 모듈, 렌더 데이터, 렌더 패스 보조 타입, D3D11 구현 소스

        - `D3D11/`: Direct3D 11 기반 실제 렌더링 백엔드 구현 소스

        - `RenderAsset/`: GPU 리소스화된 텍스처 / 폰트 / Atlas 등 렌더 전용 에셋 표현 소스

        - `Submitte/`: 씬 / 그리드 / 월드 축 / 오버레이 메시 등을 렌더러에게 제출하는 변환 / 제출 소스

        - `Types/`: 렌더링 관련 enum, render item, pick id, show flag 등 공용 타입 정의 소스

    - `SceneIO/`: 씬 직렬화 / 역직렬화, 씬 파일 포맷, 타입 레지스트리 관련

    - `ThirdParty/`: 엔진에서 사용하는 외부 라이브러리 포함 소스


## 4. 핵심 흐름

- 시작점

    - `Editor` 실행 파일의 진입점에서 `FEditorEngineLoop`을 생성하고 `PreInit()` -> `Run()` -> `ShutDown()` 순으로 에디터 생명 주기를 관리하는 구조

- 반복 루프 / 요청 흐름

    - 메인 루프는 `FEditorEngineLoop::Run()` 내부에서 `Tick()`을 반복 호출하는 형태

    - 매 프레임 `Application->PumpMessage()`로 OS 메시지를 처리하고, `FInputSystem`이 Win32 입력 메시지를 `FInputEvent` 큐로 변환 / 축적

    - `FEditor::Tick()`에서 입력 이벤트를 polling하여 전역 입력 라우터(`GlobalInputRouter`)에 전달하고, 소비되지 않은 이벤트만 `ViewportClient`로 넘김

        - `ViewportClient` 내부에서는 다시 입력 컨텍스트 단위로 라우팅하여 카메라 이동, 오브젝트 선택, 기즈모 조작 등을 각 컨트롤러로 위임

    - 이후 `ViewportClient`, `PanelManager`, `Scene` 각각에서 `Tick()`을 수행하고, `BuildRenderData()`로 `SceneView`, `EditorRenderData`, `SceneRenderData`를 구성

        - `SceneView`는 일종의 `ViewportCamera`의 스냅샷

    - 렌더 단계에서 `Render->BeginFrame()` -> `Render(EditorRenderData, SceneRenderData)` -> `Editor->DrawPanel` -> `Renderer->EndFrame()` 순으로 3D 장면과 ImGui UI를 함께 출력

- 종료 흐름

    - 종료 요청 시 메인 루프를 빠져나온 뒤 `ShutDown()`에서 ImGui, AssetLoader, AssetManager, Renderer, Editor, Application, InputSystem 순으로 해제

    - 창 닫기 시에는 즉시 종료하지 않고 `Editor->RequesetCloseEditor()`를 통해 dirty scene 여부를 확인한 뒤 종료하는 흐름이 있음


## 5. 핵심 데이터 / 핵심 추상화

- 주요 클래스 / 구조체와 각 책임

    - `FEditorEngineLoop`: 프로그램 전체 실행 루프, 각종 초기화 및 프레임 단위 오케스트레이션 담당

    - `FEditor`: 에디터 최상위 오케스트레이터
    
        - 씬 문서 상태, 전역 입력, 패널 및 메뉴, 설정, Contents Browser 인덱스, 렌더 데이터 생성 등을 통합 관리

    - `FEditorContext`: 여러 패널과 컨트롤러가 공유하는 에디터 공용 상태 묶음

    - `FEditorViewportClient`: 뷰포트 단위 편집 로직의 중심 객체

        - 카메라, 선택, 기즈모 컨트롤러를 보유하고 입력 처리와 렌더 데이터 생성

    - `FInputSystem`: Win32 메시지를 엔진 공용 입력 이벤트 / 상태로 변환

    - `FInputRouter`, `IInputContext`: 입력을 우선순위 기반 컨텍스트 단위로 분배하는 추상화

    - 각 `Viewport ... Controller`: 실제 카메라 이동, 선택 판정, gizmo 조작 같은 기능별 편집 동작 담당

    - `FScene`: 현재 편집 중인 월드 / 액터 집합 및 런타임 씬 데이터 보유

    - `FSceneView`: 카메라 view / projection / viewport rect 등 특정 렌더 시점의 정보 보유

    - `FEditorRenderData`: grid, world axis, gizmo, selection outline 등 editor-only 렌더링 정보

- 서로의 관계

    - `FEditorEngineLoop`가 최상위에서 에디터, 렌더러, 애플리케이션, 입력 시스템을 소유 / 연결함

    - `FEditor`는 내부에 `FEditorViewportClient`, `FPanelManager`, `FEditorContext`, 현재 `FScene`을 보유하고 있고, 매 프레임에 이들 상태를 갱신한 뒤 렌더러가 사용할 데이터로 변환

    - `FEditorViewportController`는 네비게이션, 선택, 기즈모 컨트롤러를 보유하며, 입력 컨텍스트를 통해 사용자 입력을 각 기능 컨트롤러에 전달

    - `FSceneView`는 뷰포트 카메라 상태로부터 계산되고, 이를 기반으로 `FEditorRenderData`와 `FSceneRenderData`가 함께 구성되어 최종적으로 렌더러에 전달됨

    - 전체 흐름은 대략 OS 메시지 -> `FInputSystem` -> `FInputRouter` / `IInputContext` -> `IViewportController` -> `FEditor`에서 렌더 데이터 구성 -> 렌더러 출력 구조로 이해하면 됨


## 6. 객체 생명주기

- 누가 생성하는가

    - 최상위 프로그램 객체들은 `FEditorEngineLoop::PreInit()`에서 생성

    - 에디터 내부 서브시스템은 주로 `FEditor::Create()`에서 생성 / 초기화

    - 뷰포트 하위 입력 / 컨트롤러 계층은 `FEditorViewportClient::Create()`에서 연결 / 초기화

- 누가 소유하는가

    - 프로그램 최상위 런타임 객체들은 `FEditorEngineLoop`가 소유

    - 에디터 내부 공용 상태와 편집 관련 객체들은 `FEditor`가 소유

    - 뷰포트 조작 관련 컨트롤러들은 `FEditorViewportClient`가 값 멤버 형태로 소유

    - 입력 컨텍스트들은 각 입력 라우터에 등록되지만, 실제 소유 자체는 `FEditor` 또는 `FEditorViewportClient` 쪽에서 값 멤버로 소유

    - 현재 편집 중인 월드 데이터는 `FEditor`가 `FScene` 포인터로 소유

- 누가 제거 / 해제하는가

    - 최상위 객체 해제는 `FEditorEngineLoop::ShutDown()`에서 수행

    - 에디터 내부 자원 정리는 `FEditor::Release()`에서 수행

    - 뷰포트 내부 동적 입력 라우터는 `FEditorViewportClient::Release()`에서 삭제

    - 씬 교체 시 기존 씬은 `FScene::ReplaceCurrentScene()` 경로에서 교체 / 제거되는 구조

- 주의할 수명 이슈

    - 에디터 컨텍스트, 각 컨트롤러, 각 입력 컨텍스트가 서로 raw pointer로 연결되어 있어, 해제 순서가 어긋나면 dangling pointer 위험이 있음

    - `FEditorViewportClient`가 값 멤버로 들고 있는 내부 컨트롤러들은 외부에서 이들을 가리키는 포인터를 들고 있으므로 `Release()` 시점 이후 참조에 주의 필요

    - `FEditor`가 현재 씬을 raw pointer로 직접 소유하고 있고, `delete`하는 구조라서 씬 포인터를 외부에 캐싱해두는 코드는 씬 교체 후 생길 수 있는 dangling pointer에 주의할 것


## 7. 내 작업 관련 경로

### 7.1. Object Iterator 구현

- 시작 함수: `/Engine/Source/CoreUObject/Object.cpp`의 전역 `GUObjectArray`

- 상위 호출자: 직접적인 상위 호출자는 없고, `for (TObjectIterator<UStaticMesh> It; It; ++It)`같은 호출에서 사용

- 하위 영향 대상: `GUObjectArray`를 읽는 방식에 영향 받음

- 관련 파일 목록

    - `/Engine/Source/CoreUObject/Object.h`

    - `/Engine/Source/CoreUObject/Object.cpp`


### 7.2. Overlay Stat 렌더링 구현

- 시작 함수

    - `FConsolePanel::ExecuteCommand()`

    - `FEditorViewportClient::DrawViewportOverlay()`

- 상위 호출자

    - 콘솔 명령 입력 경로

        - `FConsolePanel::DrawInputRow()`

        - `FConsolePanel::SubmitInput()`

        - `FConsolePanel::ExecuteCommand()`

    - 프레임 렌더 경로

        - `FEditorEngineLoop::RunFrameOnceWithoutResize()`

        - `FEditor::DrawPanel()`

        - `FEditorViewportClient::DrawViewportOverlay()`

- 하위 영향 대상

    - viewport 단위 stat 표시 상태 저장 구조 추가

    - `stat fps`, `stat memory`, `stat none` 명령 처리

    - ImGui draw list 기반 텍스트 overlay 렌더링

    - FPS / frame time 표시

    - memory snapshot 수집 및 표시

    - `~` 입력 시 콘솔 포커스 이동

    - 기존 `State Panel`의 FPS / 메모리 표시 제거

- 관련 파일 목록

    - `Editor/Source/Panel/ConsolePanel.h`

    - `Editor/Source/Panel/ConsolePanel.cpp`

    - `Editor/Source/Viewport/EditorViewportClient.h`

    - `Editor/Source/Viewport/EditorViewportClient.cpp`

    - `Editor/Source/Editor/EditorContext.h`

    - `Editor/Source/Editor/EditorContext.cpp`

    - `Editor/Source/Input/EditorGlobalContext.h`

    - `Editor/Source/Input/EditorGlobalContext.cpp`

    - `Editor/Source/Panel/StatePanel.h`

    - `Editor/Source/Panel/StatePanel.cpp`

    - `Engine/Source/ApplicationCore/Input/InputTypes.h`

    - `Engine/Source/ApplicationCore/Input/InputSystem.h`

    - `Engine/Source/ApplicationCore/Input/InputSystem.cpp`



## 8. 확인한 규칙

- 네이밍 규칙: PascalCase, Unreal Engine 스타일 접두사

- 로그 규칙: `UE_LOG` 매크로 사용

- 코딩 스타일: 기본적으로 LLVM 스타일


## 9. 아직 헷갈리는 점

- (버그) 오브젝트 선택 로직이 AABB 성공 후 삼각형 교차 검사를 거치지 않는 부분

- ...


## 10. 다음에 확인할 것

- ...


## 11. 용어 정리

- **Primitive**: 가장 낮은 수준의 렌더링 단위로, GPU 입장으로 보면 점, 선, 삼각형과 같은 rasterisation 최소 단위

    - Primitive는 모양이 아니라 일종의 조립 규칙

- **Geometry**: Primitive보다 상위 개념으로, 일반적으로 정점과 인덱스의 집합을 의미함

    - GPU에 올릴 수 있는 정점 데이터 덩어리

- **Mesh**: Geometry보다 한 단계 위 개념으로, geometry + material + submesh 구조

    - 렌더링 가능한 하나의 모델 단위

- **World**: 게임의 전체 상태

    - 물리 시스템, scene 리스트, object 리스트 등

- **Scene**: 하나의 공간 단위

    - Main scene, Menu Scene 등

- **Level**: 보통 scene과 동일하거나 하위 개념

- **Shader**: GPU에서 돌아가는 프로그램

- **Material**: Shader + parameter 묶음

    - Shader에 전달하는 데이터 묶음으로, 같은 shader라도 material에 따라 달라짐

- **Texture**: 셰이더가 샘플링하는 이미지 데이터

- **Atlas**: 여러 texture를 하나로 합친 큰 texture

- **Viewport**: 어떤 공간 전체가 아니라 그 일부를 보여주는 창(window)

    - 렌더링에만 국한된 개념은 아님

- **Editor Chrome**: 에디터 창의 타이틀바 + 메뉴바 + 시스템 버튼 같은 창 외곽 UI 프레임을 담당하는 레이어

- **Output Sink**: 생성된 로그 데이터를 최종적으로 받아서 저장하거나 출력하는 대상 객체

- **RHI(Render Hardware Interface)**: DirectX같은 GPU API를 추상화해서 엔진이 플랫폼 독립적으로 렌더링할 수 있게 만드는 인터페이스 계층

- **Context**: 어떤 작업이 수행되기 위해 필요한 현재 상태(environment state)의 묶음

- **Camera Nevigation**: 카메라 이동, 회전, 확대 / 축소 등을 통한 장면 탐색 조작 전반

- **PCH(PreCompiled Header)**: 자주 포함되는 헤더들을 미리 컴파일해 두고 재사용해서 빌드 시간을 줄이는 메커니즘

- **Registry**: 중앙에서 매핑 정보를 관리하는 테이블

- ...

---

## 체크리스트

### 실행/환경

- [x] 빌드가 되는가

- [x] 실행이 되는가

- [x] 실행 인자 / 설정 파일 / 리소스 경로를 아는가

- [x] 디버그 실행이 되는가


### 구조

- [x] 엔트리 포인트를 찾았는가

- [x] 핵심 루프 또는 핵심 요청 흐름을 찾았는가

- [x] 주요 디렉터리의 역할을 대략 설명할 수 있는가

- [x] 핵심 데이터 구조를 찾았는가


### 책임

- [x] 객체 생성 책임이 어디 있는가

- [x] 객체 소유권이 어떻게 되는가

- [x] 상태 변경이 어디서 일어나는가

- [x] 출력 / 렌더 / 응답의 최종 지점이 어디인가


### 작업 관련

- [x] 내가 수정할 기능의 시작점이 어디인가

- [x] 관련 클래스 / 함수를 찾았는가

- [x] 상위 호출자와 하위 영향 범위를 대략 아는가

- [x] 작은 수정 하나는 안전하게 할 수 있는가


### 협업

- [x] 팀의 네이밍 / 코딩 스타일을 파악했는가

- [x] 로그 / 디버깅 방식이 무엇인지 아는가

- [x] 질문할 때 사용할 최소한의 구조 지도가 있는가
