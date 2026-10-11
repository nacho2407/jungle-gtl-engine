# RmlUi 기반 코어 UI 시스템 기획서

## 1. 개요

본 문서는 `KraftonEngine`에서 RmlUi 라이브러리를 활용해 런타임 게임 UI를 체계적으로 구현하기 위한 코어 UI 시스템 기획서이다.

대상 UI는 다음과 같다.

- 타이틀 화면
- HUD: 체력, 조준점, 탄약, 목표, 상호작용 프롬프트
- 대사창 / 자막 / 통신창
- 아이템 조사 창
- 팝업 / 확인 창
- 일반 위젯
- 게임 Pause 창
- 게임 Settings 창
- 디버그용 치트 창

현재 엔진은 RmlUi 기반 UI를 완전히 새로 만들어야 하는 상태가 아니다. 이미 `UUIManager`, `UUserWidget`, RmlUi D3D11 렌더 인터페이스, Lua UI 바인딩이 존재한다. 따라서 목표는 신규 시스템을 처음부터 작성하는 것이 아니라, 현재 구조 위에 `UI Stack`, `Input Policy`, `ViewModel`, `Screen Flow`를 얹어 게임 전체 UI를 안정적으로 관리할 수 있게 확장하는 것이다.

## 2. 현재 엔진 UI 상태

현재 확인된 구조는 다음과 같다.

- `UUIManager`
  - RmlUi 초기화 및 종료 담당
  - `Rml::Context` 생성
  - RML 문서 로드 및 렌더링
  - Viewport에 올라간 `UUserWidget` 목록 관리
  - 마우스 이동 및 좌클릭 입력 전달

- `UUserWidget`
  - RML 문서 하나를 감싸는 UObject 래퍼
  - Lua에서 생성 가능
  - `BindClick`, `SetText`, `SetProperty`, `SetWantsMouse` 지원

- Lua 바인딩
  - `UI.CreateWidget(path)`
  - `widget:AddToViewportZ(z)`
  - `widget:bind_click(id, callback)`
  - `widget:SetText(id, text)`
  - `widget:SetProperty(id, property, value)`
  - `Engine.PauseGame()`, `Engine.ResumeGame()`, `Engine.Exit()`, `Engine.SetOnEscape()`

- 최근 추가된 타이틀 UI 예시
  - `Content/UI/Title/Title.rml`
  - `Content/UI/Title/Title.rcss`
  - `Content/UI/Title/StatIcon.png`
  - `Content/Script/TitleScreen.lua`
  - `Content/Scene/Title.Scene`

현재 구조는 MVP UI 구현에는 충분하지만, 여러 창이 동시에 열리는 게임 UI에는 아직 부족하다. 특히 Pause, Settings, Popup, Item Inspect처럼 모달 UI가 생기면 입력 차단, ESC 처리, 게임 일시정지, 마우스 표시 정책이 얽히기 쉽다.

## 3. 목표

### 3.1 기능 목표

코어 UI 시스템은 다음 기능을 제공해야 한다.

- 화면 단위 UI 생성, 표시, 닫기
- 레이어별 Z-order 관리
- 모달 UI 관리
- ESC / Back 입력 처리
- 게임 입력과 UI 입력 분리
- 마우스 표시, 커서 캡처, raw mouse 사용 여부 제어
- UI 표시 중 게임 일시정지 여부 제어
- HUD 데이터 갱신 구조 제공
- 팝업, 설정창, 치트창 같은 재사용 UI 제공
- Lua에서 간단히 UI를 열고 닫을 수 있는 API 제공

### 3.2 비목표

초기 버전에서 다음은 우선순위에서 제외한다.

- 완전한 RmlUi Data Binding 전면 도입
- 모든 UI의 게임패드 완전 지원
- 복잡한 UI 애니메이션 시스템
- UI 에디터 제작
- 3D 모델을 직접 회전하는 아이템 조사 뷰어

초기에는 수동 `SetText`, `SetProperty`, `BindClick` 기반으로 빠르게 완성하고, HUD와 Settings부터 ViewModel 구조를 도입한다.

## 4. 핵심 설계

### 4.1 전체 구조

추천 구조는 다음과 같다.

```text
UUIManager
 ├─ FRmlSystemInterface
 ├─ FRmlFileInterfaceWide
 ├─ FRmlRenderInterfaceD3D11
 ├─ Rml::Context* GameViewport
 ├─ FUIStack
 ├─ FUIInputRouter
 ├─ FUIModelStore
 └─ UUserWidget instances
```

`UUserWidget`은 RML 문서 하나의 래퍼로 유지한다. 대신 `UUIManager`가 단순히 `ViewportWidgets`를 정렬해서 그리는 역할에서 확장되어, UI Stack과 입력 정책을 통합 관리한다.

### 4.2 UI Layer

모든 UI는 레이어를 가진다. 레이어는 화면 우선순위와 입력 우선순위를 함께 결정한다.

```cpp
enum class EUILayer
{
    Title        = 0,
    HUD          = 100,
    Crosshair    = 150,
    Interaction  = 200,
    Dialogue     = 300,
    Boss         = 400,
    Menu         = 500,
    Settings     = 600,
    Popup        = 700,
    Debug        = 900,
};
```

레이어 사용 예시는 다음과 같다.

| UI | Layer |
| --- | --- |
| 타이틀 / 엔딩 | Title |
| HUD | HUD |
| 조준점 | Crosshair |
| 상호작용 프롬프트 | Interaction |
| 대사창 / 자막 | Dialogue |
| 보스 체력 / 카운트다운 | Boss |
| Pause | Menu |
| Settings | Settings |
| 팝업 | Popup |
| 치트창 | Debug |

### 4.3 Input Mode

현재는 `WantsMouse` 하나로 마우스 표시 여부만 판단한다. 코어 UI에서는 입력 모드를 명확히 분리한다.

```cpp
enum class EUIInputMode
{
    GameOnly,
    GameAndUI,
    UIOnly,
    ModalUI,
    DebugUI,
};
```

입력 모드별 정책은 다음과 같다.

| UI | InputMode | 마우스 | 게임 입력 | Pause |
| --- | --- | --- | --- | --- |
| HUD / 조준점 | GameOnly | 숨김 | 허용 | X |
| 상호작용 프롬프트 | GameAndUI | 숨김 | 허용 | X |
| 대사창 | GameAndUI 또는 ModalUI | 상황별 | 상황별 | 상황별 |
| 아이템 조사창 | ModalUI | 표시 | 차단 | O |
| 타이틀 | UIOnly | 표시 | 차단 | O 또는 World 없음 |
| Pause | UIOnly | 표시 | 차단 | O |
| Settings | ModalUI | 표시 | 차단 | O |
| Popup | ModalUI | 표시 | 차단 | O |
| 치트창 | DebugUI | 표시 | 선택 | 보통 X |

### 4.4 UI Stack Entry

UI Stack은 열린 화면들을 스택 형태로 관리한다.

```cpp
struct FUIStackEntry
{
    UUserWidget* Widget = nullptr;
    EUILayer Layer = EUILayer::HUD;
    int32 ZOrder = 0;
    EUIInputMode InputMode = EUIInputMode::GameOnly;
    bool bModal = false;
    bool bPausesGame = false;
    bool bBlocksGameInput = false;
    bool bWantsMouse = false;
};
```

`UUIManager`는 최상위 UI Stack Entry를 기준으로 현재 입력 정책을 계산한다.

```cpp
struct FUIInputPolicy
{
    bool bShowCursor = false;
    bool bCaptureCursor = true;
    bool bUseRawMouse = true;
    bool bBlockGameInput = false;
    bool bPauseWorld = false;
};
```

### 4.5 주요 API

C++ 측 핵심 API는 다음과 같이 설계한다.

```cpp
UUserWidget* PushWidget(
    const FString& DocumentPath,
    EUILayer Layer,
    EUIInputMode InputMode,
    bool bModal,
    bool bPausesGame);

void PopWidget(UUserWidget* Widget);
void CloseLayer(EUILayer Layer);
void CloseAllModal();
bool HandleBack();
FUIInputPolicy GetInputPolicy() const;
```

Lua 측 API는 단순하게 제공한다.

```lua
UI.PushScreen("Content/UI/Menu/Pause.rml", "Menu", "UIOnly")
UI.PopScreen(widget)
UI.ShowPopup(title, message, onConfirm, onCancel)
UI.OpenPause()
UI.OpenSettings()
UI.CloseTop()
```

초기 구현에서는 문자열 기반 enum 변환을 허용하고, 안정화 후 C++ enum 바인딩으로 정리한다.

## 5. 입력 처리 설계

### 5.1 현재 한계

현재 `UUIManager::ProcessInput()`은 다음 입력만 처리한다.

- 마우스 이동
- 좌클릭 Down
- 좌클릭 Up

향후 UI 사용을 위해 다음 입력을 추가해야 한다.

- 마우스 휠
- 우클릭 / 중클릭
- 키보드 KeyDown / KeyUp
- 텍스트 입력
- 게임패드 포커스 이동
- ESC / Back 처리

### 5.2 입력 정책 적용

`GameViewportClient`는 현재 `AnyViewportWidgetWantsMouse()`에 의존한다. 이를 `UUIManager::GetInputPolicy()` 기반으로 변경한다.

```cpp
FUIInputPolicy Policy = UUIManager::Get().GetInputPolicy();

SetCursorVisible(Policy.bShowCursor);
SetCursorCaptured(Policy.bCaptureCursor);
InputSystem::Get().SetUseRawMouse(Policy.bUseRawMouse);

if (Policy.bBlockGameInput)
{
    ClearGameInputSnapshot();
}
```

이 정책을 통해 “UI 버튼도 눌리고 동시에 총도 발사되는 문제”를 방지한다.

### 5.3 ESC / Back 정책

ESC 입력 처리 순서는 다음과 같다.

1. 열려 있는 Popup이 있으면 Popup 닫기
2. Settings가 열려 있으면 Settings 닫기
3. Item Inspect가 열려 있으면 Item Inspect 닫기
4. Pause가 열려 있으면 Resume
5. 아무것도 없으면 Pause 열기

```cpp
bool UUIManager::HandleBack()
{
    if (CloseTopModal()) return true;
    if (IsPauseOpen())
    {
        ClosePause();
        return true;
    }

    OpenPause();
    return true;
}
```

현재 Lua에는 `Engine.SetOnEscape()`가 있으므로, MVP 단계에서는 Lua에서 동일 정책을 먼저 구현하고 이후 C++ `UUIManager::HandleBack()`으로 이전한다.

## 6. ViewModel 설계

### 6.1 MVP 방식

초기에는 현재 구조를 유지한다.

```lua
hud:SetText("health-text", "100 / 100")
hud:SetText("ammo-text", "30 / 90")
hud:SetProperty("interaction", "display", "block")
```

이 방식은 타이틀, 팝업, 간단한 Pause 창에는 충분하다.

### 6.2 확장 방식

HUD처럼 자주 바뀌는 UI는 ViewModel로 확장한다.

```cpp
struct FHUDViewModel
{
    int Health = 100;
    int MaxHealth = 100;
    int Ammo = 30;
    int ReserveAmmo = 90;
    bool bZooming = false;
    FString ObjectiveText;
    FString InteractionText;
};
```

대사창과 Settings도 별도 ViewModel을 가진다.

```cpp
struct FDialogueViewModel
{
    bool bVisible = false;
    FString Speaker;
    FString Body;
    FString PortraitPath;
};

struct FSettingsViewModel
{
    float MouseSensitivity = 1.0f;
    float MasterVolume = 1.0f;
    bool bHoldToZoom = false;
};
```

중장기적으로는 RmlUi Data Binding을 사용해 직접 `SetText()`를 반복 호출하지 않는 구조로 전환한다.

## 7. 화면별 기획

### 7.1 타이틀 화면

현재 타이틀 화면은 이미 다음 리소스로 구현 가능하다.

- `Content/Scene/Title.Scene`
- `Content/UI/Title/Title.rml`
- `Content/UI/Title/Title.rcss`
- `Content/UI/Title/Title.png`
- `Content/UI/Title/StatIcon.png`
- `Content/Script/TitleScreen.lua`

기능:

- `FRACTURE LINE` 제목 표시
- `START GAME`, `SETTINGS`, `CREDITS`
- `NEW GAME`, `CONTINUE`, `QUIT`
- Stat 아이콘 클릭 시 스코어보드 패널 표시
- Quit 클릭 시 종료 확인 팝업 표시

추가 필요:

- `Engine.TransitionToScene(sceneName)` Lua 바인딩
- Settings 화면 열기
- Credits 화면 열기

### 7.2 HUD

HUD는 항상 켜져 있고 마우스를 요구하지 않는다.

리소스:

```text
Content/UI/HUD/HUD.rml
Content/UI/HUD/HUD.rcss
```

표시 요소:

- 체력
- 탄약
- 조준점
- 현재 목표
- 상호작용 프롬프트
- 재장전 상태
- 보스 체력바
- LANCER 카운트다운

HUD 생성 예시:

```lua
local hud = UI.CreateWidget("Content/UI/HUD/HUD.rml")
hud:SetWantsMouse(false)
hud:AddToViewportZ(100)
```

조준점은 RmlUi 중앙 고정 element로 구현한다.

```xml
<div id="crosshair">
  <div class="dot"></div>
</div>
```

무기 반동이나 확산은 class 변경으로 표현한다.

### 7.3 대사창

대사창은 Dialogue System과 연결한다.

리소스:

```text
Content/UI/Dialogue/Dialogue.rml
Content/UI/Dialogue/Dialogue.rcss
```

기능:

- 화자 이름
- 대사 본문
- 통신창 스타일
- 선택지
- Skip / Next

FRACTURE LINE 사용 예:

- ACT 1: 드레이크 브리핑
- ACT 2: LANCER 로그 발견
- ACT 3: 레아 전투 중 외침
- ACT 4: 드레이크 배신 통신
- ACT 5: LANCER 카운트다운
- Ending: VANTUS 패치 제거

게임잼 범위에서는 풀 컷신보다 HUD 자막 + 통신창 방식이 비용 대비 효율적이다.

### 7.4 아이템 조사 창

아이템 조사 창은 ModalUI로 동작한다.

리소스:

```text
Content/UI/ItemInspect/ItemInspect.rml
Content/UI/ItemInspect/ItemInspect.rcss
```

동작:

1. 플레이어가 아이템 앞에서 E 입력
2. World Pause
3. Item Inspect 표시
4. 마우스 표시
5. 닫기 버튼 또는 ESC
6. World Resume

표시 요소:

- 아이템 이미지
- 아이템 이름
- 설명
- 획득 여부
- 닫기 버튼

초기 구현에서는 3D 회전 모델 대신 이미지 + 설명으로 구성한다.

### 7.5 Popup

Popup은 범용 확인 창으로 만든다.

리소스:

```text
Content/UI/Menu/Popup.rml
Content/UI/Menu/Popup.rcss
```

사용 예:

- 게임 종료 확인
- 설정 저장 확인
- 체크포인트 재시작
- 데이터 드라이브 획득

설계:

```cpp
struct FUIPopupDesc
{
    FString Title;
    FString Message;
    FString ConfirmText = "OK";
    FString CancelText;
    bool bHasCancel = false;
    std::function<void()> OnConfirm;
    std::function<void()> OnCancel;
};
```

Popup은 항상 `EUILayer::Popup`에 올라간다.

### 7.6 Pause 창

리소스:

```text
Content/UI/Menu/Pause.rml
Content/UI/Menu/Pause.rcss
```

기능:

- Resume
- Settings
- Restart Checkpoint
- Return to Title
- Quit

Pause가 열리면 다음 정책을 적용한다.

- 마우스 표시
- 게임 입력 차단
- World Pause
- ESC로 닫기

### 7.7 Settings 창

리소스:

```text
Content/UI/Menu/Settings.rml
Content/UI/Menu/Settings.rcss
```

초기 옵션:

- Mouse Sensitivity
- Invert Y
- Hold / Toggle Zoom
- Master Volume
- Fullscreen / Windowed
- Resolution
- Apply
- Back

Settings 저장은 `ProjectSettings.ini`와 분리한다.

```text
KraftonEngine/Saves/Config/GameUserSettings.json
```

Settings는 폼 입력이 필요하므로 RmlUi keyboard/text input 처리가 반드시 확장되어야 한다.

### 7.8 디버그용 치트 창

리소스:

```text
Content/UI/Debug/CheatWindow.rml
Content/UI/Debug/CheatWindow.rcss
```

키:

- F1 또는 Backtick

기능:

- God Mode
- Give Ammo
- Teleport Checkpoint
- Load Act
- Spawn Enemy
- Kill All Enemies
- Show Collision
- Show FPS

런타임 빌드에서도 사용할 수 있어야 하므로 RmlUi로 구현한다. 에디터 전용 디버그 도구는 ImGui를 유지한다.

## 8. 파일 구조

추천 UI 파일 구조:

```text
Content/UI/
  Common/
    theme.rcss
    fonts.rcss
    layout.rcss
    buttons.rcss
  Title/
    Title.rml
    Title.rcss
    Title.png
    StatIcon.png
  HUD/
    HUD.rml
    HUD.rcss
  Dialogue/
    Dialogue.rml
    Dialogue.rcss
  ItemInspect/
    ItemInspect.rml
    ItemInspect.rcss
  Menu/
    Pause.rml
    Pause.rcss
    Settings.rml
    Settings.rcss
    Popup.rml
    Popup.rcss
  Debug/
    CheatWindow.rml
    CheatWindow.rcss
```

공통 스타일은 반드시 분리한다.

```xml
<head>
  <link type="text/rcss" href="../Common/theme.rcss"/>
  <link type="text/rcss" href="HUD.rcss"/>
</head>
```

## 9. 구현 로드맵

### 1단계: UI Stack

가장 먼저 `UUIManager`에 Stack 구조를 추가한다.

구현 항목:

- `FUIStackEntry`
- `FUIInputPolicy`
- `PushWidget`
- `PopWidget`
- `CloseLayer`
- `CloseAllModal`
- `HandleBack`
- `GetInputPolicy`

이 단계가 완료되면 Title, Pause, Popup, Settings, Item Inspect가 같은 규칙으로 열린다.

### 2단계: Input Policy

`WantsMouse` 중심 구조에서 입력 정책 중심 구조로 전환한다.

구현 항목:

- 마우스 표시 여부
- 커서 캡처 여부
- raw mouse 사용 여부
- 게임 입력 차단 여부
- World pause 여부

### 3단계: RmlUi 입력 확장

현재 좌클릭 중심 입력을 확장한다.

구현 항목:

- Mouse Wheel
- Right / Middle Mouse Button
- KeyDown
- KeyUp
- TextInput
- ESC / Back

Settings와 Debug Cheat Window를 위해 키보드 입력 처리가 중요하다.

### 4단계: Title / HUD / Pause / Popup

최소 출시 UI를 구현한다.

필수 UI:

- Title
- HUD
- Pause
- Popup

### 5단계: Dialogue / ItemInspect / Settings

게임 흐름 UI를 구현한다.

필수 UI:

- Dialogue
- ItemInspect
- Settings

### 6단계: Debug Cheat Window

개발 편의 기능을 구현한다.

필수 기능:

- God Mode
- Give Ammo
- Load Act
- Spawn Enemy
- Show Collision

### 7단계: HUD ViewModel

HUD부터 ViewModel 구조를 도입한다.

이후 Dialogue, Settings, Popup에도 확장한다.

## 10. FRACTURE LINE UI 플로우

권장 전체 플로우는 다음과 같다.

```text
Boot
 → Title.Scene + Title.rml
 → Start 클릭
 → Act1_Training.Scene
 → HUD 표시
 → 조작 튜토리얼 프롬프트 표시
 → ACT2부터 Objective / HUD 갱신
 → 로그 조사 시 ItemInspect 표시
 → 레아 / 드레이크 대사는 Dialogue 표시
 → 보스전에서 BossHealth + LANCER Countdown 표시
 → 엔딩에서 Ending.rml 표시
```

씬 전환 전후에는 UI 정리가 필요하다.

```cpp
UUIManager::Get().ClearGameplayUI();
```

또는 지속 UI와 씬 종속 UI를 분리한다.

```cpp
UUIManager::Get().CloseAllExceptPersistent();
```

## 11. 기술 리스크

### 11.1 입력 중복

UI 버튼 클릭과 게임 입력이 동시에 발생할 수 있다.

대응:

- `FUIInputPolicy::bBlockGameInput`
- GameViewportClient에서 게임 입력 snapshot clear

### 11.2 ESC 정책 충돌

Pause, Popup, Settings, Item Inspect가 동시에 열릴 수 있다.

대응:

- `UUIManager::HandleBack()` 단일 진입점 사용
- 최상위 Modal 우선 닫기

### 11.3 씬 전환 시 UI 잔존

씬 전환 후 이전 UI가 남을 수 있다.

대응:

- Scene Transition 전 `ClearGameplayUI`
- Persistent UI와 Scene UI 분리

### 11.4 이벤트 리스너 수명

RML 문서를 닫은 뒤 Lua 콜백과 이벤트 리스너가 남으면 크래시 위험이 있다.

현재 `UUserWidget::ClearEventListeners()` 후 `Document->Close()`하는 방향은 적절하다. 향후 Stack 구조에서도 이 순서를 유지한다.

### 11.5 UI 렌더 패스 위치

현재 UI가 최종 감마 보정 전후 어디에 합성되는지 명확히 결정해야 한다.

추천:

- UI는 후처리와 감마 보정 이후에 최종 합성하는 것을 우선 검토
- 현재 패스 위치를 유지한다면 sRGB / linear 처리를 명확히 정리

## 12. 최종 권장 방향

이번 프로젝트에서는 다음 원칙으로 구현한다.

- RmlUi는 런타임 게임 UI 전용
- ImGui는 에디터 / 개발툴 전용
- `UUserWidget`은 RML document wrapper로 유지
- `UUIManager`는 UI Stack, Input Policy, Render Context를 소유
- Lua는 화면 흐름과 버튼 이벤트 작성에 사용
- C++은 HUD 데이터, 게임 상태, 치트 기능, 씬 전환 API를 제공

우선 구현해야 할 핵심은 다음 네 가지이다.

```text
FUIStackEntry
FUIInputPolicy
UUIManager::PushWidget()
UUIManager::HandleBack()
```

이 네 가지가 안정화되면 타이틀, HUD, Pause, Settings, Popup, Dialogue, Item Inspect, Cheat Window를 모두 같은 규칙으로 운용할 수 있다.
