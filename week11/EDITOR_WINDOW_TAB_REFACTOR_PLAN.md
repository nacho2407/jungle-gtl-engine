# Editor Window / Tab Manager Refactor Plan

## 1. Goal

Level Editor, Static/Skeletal Mesh Previewer, Material Editor, Curve Editor, Actor Sequencer를 UE5처럼 **창/탭 단위**로 관리한다.

핵심은 탭 UI만 만드는 것이 아니다. 활성 탭의 맥락에 따라 다음 요소가 모두 바뀌어야 한다.

```text
Active Tab Context
  -> Menu Bar
  -> Toolbar / PIE Bar
  -> Shortcut Mapping
  -> Save Target
  -> Input Routing
  -> Focused Viewport
```

예시:

```text
Level Editor active
  Ctrl+S = Save Scene
  Toolbar = Save Scene / PIE / Stop / Viewport tools
  Menu = File, Edit, Window, Build, Settings

Skeletal Mesh Viewer active
  Ctrl+S = Save SkeletalMesh asset
  Toolbar = Save Mesh / Show Bones / Reset Pose / Socket tools
  Menu = File, Asset, View, Window

Material Editor active
  Ctrl+S = Save Material or Material Instance
  Toolbar = Save Material / Apply / Revert / Preview Mesh
  Menu = File, Asset, View, Window
```

Level Editor는 항상 존재하는 고정 탭이며 닫을 수 없다.
나머지는 asset path 또는 대상 UUID 기준으로 열리는 document/tool tab이다.

---

## 2. Current Structure

현재 구조:

```text
FEditorMainPanel
  - RenderViewportHostWindow()
  - RenderEditorPanelWindows()
  - Widgets.MaterialWidget
  - Widgets.CurveEditorWidget
  - Widgets.ActorSequencerWidget
  - Widgets.ViewerWindowWidgets[]

UEditorEngine
  - TArray<unique_ptr<FEditorViewer>> Viewers
  - CreateViewer()
  - RemoveViewer()

FEditorViewerWindowWidget
  - Viewer ImGui window
  - Preview World / Preview Viewport / Skeleton Tree / Bone Details

FEditorMaterialWidget
  - Single Material Editor window
  - Asset material or component material slot

FEditorCurveEditorWidget
  - Single Curve Editor window

FEditorActorSequencerWidget
  - Single Actor Sequencer window
```

현재 장점:

```text
- Viewer는 이미 Level World와 분리된 Preview World를 가진다.
- Skeletal Viewer는 skeleton tree, socket editing, bone details 기능이 어느 정도 모여 있다.
- Material / Curve / Sequencer 기능도 이미 독립 widget으로 나뉘어 있다.
```

현재 문제:

```text
- Open / close / focus 규칙이 widget마다 다르다.
- Material Editor, Curve Editor, Actor Sequencer는 전역 singleton widget처럼 동작한다.
- Viewer는 별도 배열, 나머지는 MainPanel visibility bool에 묶여 있다.
- Ctrl+S, File > Save, toolbar button의 의미가 활성 작업 맥락에 따라 바뀌지 않는다.
- PIE Bar가 Level Editor 전용 도구인지, editor 전체 도구인지 경계가 흐려질 수 있다.
- Active tab과 focused viewport, keyboard focus가 명시적으로 분리되어 있지 않다.
```

---

## 3. Target Structure

1차 목표는 과한 상속 구조가 아니라, 작고 명확한 tab manager로 시작한다.

```cpp
enum class EEditorTabKind
{
    LevelEditor,
    StaticMeshViewer,
    SkeletalMeshViewer,
    MaterialEditor,
    CurveEditor,
    ActorSequencer,
    RuntimeUIPreview,
    ProjectSettings,
    WorldSettings,
};

struct FEditorTabId
{
    FString Value;
};

class IEditorTab
{
public:
    virtual ~IEditorTab() = default;

    virtual FEditorTabId GetTabId() const = 0;
    virtual FString GetTitle() const = 0;
    virtual EEditorTabKind GetKind() const = 0;

    virtual bool IsCloseable() const { return true; }
    virtual void Tick(float DeltaTime) {}
    virtual void Render(float DeltaTime) = 0;

    virtual void OnActivated() {}
    virtual void OnDeactivated() {}
    virtual void OnClose() {}
};
```

Tab manager:

```cpp
class FEditorTabManager
{
public:
    void InitializeDefaultTabs();

    void OpenOrFocusTab(
        const FEditorTabId& TabId,
        std::function<std::unique_ptr<IEditorTab>()> Factory);

    void CloseTab(const FEditorTabId& TabId);
    void FocusTab(const FEditorTabId& TabId);

    IEditorTab* GetActiveTab();
    const FEditorTabId& GetActiveTabId() const;

    void RenderTabStrip(float DeltaTime);
    void RenderActiveTab(float DeltaTime);

private:
    TArray<std::unique_ptr<IEditorTab>> Tabs;
    FEditorTabId ActiveTabId;
};
```

---

## 4. Tab ID Rules

Tab ID는 중복 오픈 방지와 focus 이동을 위해 안정적인 문자열이어야 한다.

```text
Level Editor
  TabId = "LevelEditor"
  Closeable = false

Static Mesh Viewer
  TabId = "StaticMeshViewer:" + AssetPath

Skeletal Mesh Viewer
  TabId = "SkeletalMeshViewer:" + AssetPath

Material Editor asset mode
  TabId = "MaterialEditor:" + MaterialAssetPath

Material Editor slot mode
  TabId = "MaterialSlot:" + ActorUUID + ":" + ComponentUUID + ":" + SlotIndex

Curve Editor asset mode
  TabId = "CurveEditor:" + CurveAssetPath

Curve Editor from Actor Sequence
  TabId = "SequenceCurve:" + SequenceComponentUUID + ":" + TrackPath

Actor Sequencer
  TabId = "ActorSequencer:" + ActorSequenceComponentUUID
```

같은 `TabId`가 이미 열려 있으면 새 탭을 만들지 않고 기존 탭으로 focus한다.

---

## 5. Command Context

탭 구조에서 가장 중요한 부분이다.

`IEditorTab`은 단순히 `Render()`만 하면 부족하다. 각 탭은 자신의 command context를 제공해야 한다.

```cpp
enum class EEditorCommandId
{
    Save,
    SaveAs,
    CloseTab,
    PlayPIE,
    StopPIE,
    BuildGame,
    AddTrack,
    AddKey,
    DeleteSelection,
    ResetPreviewCamera,
    ResetPose,
    ToggleBones,
    ToggleBounds,
    ToggleGrid,
    ToggleSockets,
    ToggleBonePicking,
};

struct FEditorShortcut
{
    int Key = 0;
    bool bCtrl = false;
    bool bShift = false;
    bool bAlt = false;
};

class FEditorCommandList
{
public:
    void MapAction(EEditorCommandId CommandId, FEditorShortcut Shortcut, std::function<void()> Execute);
    bool TryExecuteShortcut(const FEditorShortcut& Shortcut);
    bool TryExecuteCommand(EEditorCommandId CommandId);
};
```

Tab interface는 command/menu/toolbar/save를 노출한다.

```cpp
class IEditorTab
{
public:
    virtual void BuildMenu(FEditorMenuBuilder& Menu) {}
    virtual void BuildToolbar(FEditorToolbarBuilder& Toolbar) {}
    virtual void RegisterCommands(FEditorCommandList& Commands) {}

    virtual bool CanSave() const { return false; }
    virtual bool Save() { return false; }
};
```

Shortcut 처리 순서:

```text
1. Active modal / popup
2. Text input capture
3. Global command
4. Active tab command list
5. Focused viewport command list
```

중요 규칙:

```text
Ctrl+S는 전역 Save 함수가 아니다.
Ctrl+S는 ActiveTab->Save()를 호출한다.
```

---

## 6. Menu / Toolbar Context

상단 Menu와 Toolbar는 고정 UI가 아니라 active tab context를 따라간다.

```text
Main Menu
  = Global menu + ActiveTab menu

Main Toolbar
  = Global toolbar + ActiveTab toolbar
```

Global menu:

```text
File
Window
Help
```

Level Editor active:

```text
Menu
  File: New Scene, Open Scene, Save Scene, Save Scene As
  Edit: Undo, Redo, Duplicate, Delete
  Build: Build Game
  Window: Content Browser, Details, Outliner, Project Settings, World Settings

Toolbar / PIE Bar
  Save Scene
  Play PIE
  Stop PIE
  Possess / Eject
  Viewport Layout
  Transform Tool
```

Previewer active:

```text
Menu
  File: Save Asset, Close Tab
  Asset: Reimport, Reset Preview, Save Socket Data
  View: Show Mesh, Show Bones, Show Bounds
  Window: Content Browser, Details if needed

Toolbar
  Save Asset
  Reset Camera
  Show Options
  Reset Pose
```

Material Editor active:

```text
Menu
  File: Save Material, Close Tab
  Asset: Create Instance, Apply, Revert
  View: Preview Mesh, Preview Lighting

Toolbar
  Save Material
  Apply
  Revert
  Preview Mesh Selector
```

Actor Sequencer active:

```text
Menu
  File: Save Scene or Save Sequence
  Edit: Undo, Redo, Delete Key
  Track: Add Component Track, Add Property Track, Add Key

Toolbar
  Add Track
  Add Key
  Play Sequence
  Pause
  Stop
  FPS
```

---

## 7. Save Semantics

Save는 탭별로 의미가 다르다.

```text
Level Editor
  Ctrl+S = Save current scene

Static Mesh Viewer
  Ctrl+S = Save static mesh asset or metadata

Skeletal Mesh Viewer
  Ctrl+S = Save skeletal mesh asset, sockets, preview metadata

Material Editor
  Ctrl+S = Save material or material instance

Curve Editor asset mode
  Ctrl+S = Save .curve asset

Curve Editor from Actor Sequence
  Ctrl+S = Save owning scene or sequence owner

Actor Sequencer
  Ctrl+S = Save owning scene for ActorSequenceComponent
```

주의:

```text
ActorSequence가 Scene-owned라면 Save Sequence는 실제로 Scene Save다.
나중에 LevelSequence asset을 도입하면 Save target을 Sequence asset으로 바꿀 수 있어야 한다.
```

---

## 8. Focus Model

Focus는 최소 네 층으로 분리한다.

```text
1. Active Editor Tab
2. Focused ImGui Window
3. Focused Viewport Client
4. Runtime Input Owner
```

권장 상태:

```cpp
struct FEditorFocusState
{
    FEditorTabId ActiveTabId;
    FEditorTabId HoveredTabId;
    FEditorTabId KeyboardFocusTabId;

    FEditorViewportClient* FocusedViewportClient = nullptr;
    FEditorViewportClient* HoveredViewportClient = nullptr;

    APlayerController* RuntimeInputOwner = nullptr;

    bool bAnyModalOpen = false;
    bool bAnyTextInputActive = false;
    bool bPIEPossessed = false;
};
```

규칙:

```text
- 탭 클릭 시 ActiveTabId 변경
- Viewport canvas 클릭 시 FocusedViewportClient 변경
- ImGui input text / popup / modal이 열려 있으면 viewport input 차단
- PIE possessed 상태에서는 Level Editor viewport만 gameplay input 허용
- Previewer viewport는 preview camera input만 허용
- Material preview viewport는 material preview camera input만 허용
```

중요:

```text
ActiveTab == FocusedViewportClient는 항상 성립하지 않는다.
예를 들어 Level Editor 탭이 active여도 Content Browser 검색창이 focus되면 viewport input은 막혀야 한다.
```

---

## 9. Input Routing Target

목표 흐름:

```text
Windows/InputSystem
  -> ShortcutRouter
      -> Global commands
      -> Active tab commands

  -> FEditorInputRouter
      -> FocusState.FocusedViewportClient
          -> Level Editor viewport
          -> Viewer viewport
          -> Material preview viewport
```

PIE possessed:

```text
Level Editor viewport
  -> GameInputBridge
  -> PlayerController
  -> Pawn
```

Previewer:

```text
Viewer viewport
  -> Viewer camera controller
  -> Preview World only
```

Material Editor:

```text
Material preview viewport
  -> Material preview camera controller
  -> Preview Scene only
```

---

## 10. Tab Responsibilities

### Level Editor Tab

```text
- Scene / World editing
- Main viewport layout
- Outliner
- Details
- Content Browser
- Control panel
- PIE
- World Settings / Project Settings
```

닫기 불가.

### Static Mesh Viewer Tab

```text
- Static mesh preview
- Bounds / material slots / collision preview
- Preview world
- Asset save
```

### Skeletal Mesh Viewer Tab

```text
- Skeletal mesh preview
- Skeleton tree
- Bone selection
- Socket editing
- Bone debug rendering
- Bone picking when Bones show flag is enabled
- Asset save
```

### Material Editor Tab

```text
- Material asset editing
- Material instance editing
- Component material slot editing
- Preview mesh / light / camera
- Parameter editing
```

Slot mode는 actor/component 삭제에 안전해야 한다.

### Curve Editor Tab

```text
- Curve asset editing
- Sequence-owned curve editing
- Key / tangent editing
```

### Actor Sequencer Tab

```text
- ActorSequenceComponent editing
- Component / property track creation
- Key add/delete/move
- Playback start/end/playhead editing
- Curve Editor selection sync
```

---

## 11. Mesh Previewer Viewport Surface

Mesh Previewer도 Level Editor viewport와 같은 계열의 viewport surface로 취급한다.

즉, Previewer의 viewport toolbar는 별도의 임시 UI가 아니라 Level Editor viewport toolbar와 같은 시각 언어를 사용해야 한다.

```text
Common Viewport Toolbar Style
  - icon button
  - compact dropdown
  - Show menu
  - camera / view mode menu
  - tooltip
  - selected state
```

다만 기능 구성은 active tab context에 따라 달라진다.

```text
Level Editor Viewport Toolbar
  - Select / Move / Rotate / Scale
  - camera mode
  - grid snap
  - view mode
  - show flags
  - PIE related affordance

Static Mesh Previewer Toolbar
  - Save Asset
  - reset preview camera
  - view mode
  - show flags
  - bounds / collision / normals / tangents
  - material slot visibility

Skeletal Mesh Previewer Toolbar
  - Save Asset
  - reset preview camera
  - reset pose
  - view mode
  - show flags
  - mesh / bones / sockets / bounds / skin weight debug
  - bone picking toggle
```

구현은 toolbar style을 복사하지 않고 helper를 공유한다.

```text
FViewportToolbarRenderer
  - DrawIconButton()
  - DrawDropdownButton()
  - DrawShowMenu()
  - DrawViewModeMenu()

LevelEditorTab
  -> BuildLevelViewportToolbar()

ViewerTab
  -> BuildPreviewViewportToolbar()
```

기존 `DrawViewportIconButton`, `RenderViewportMenuBarForIndex`, `RenderViewportIconToolbarForIndex`는 Level Editor 전용 이름이 강하므로 장기적으로 공통 helper로 추출한다.

### Preview Show Flags

현재 Level Editor의 show flag와 Skeletal Viewer의 `FSkeletalViewerShowFlags`는 섞일 가능성이 있다.
최종적으로는 공통 flag와 preview-specific flag를 나눠야 한다.

```cpp
struct FPreviewViewportShowFlags
{
    bool bShowGrid = true;
    bool bShowBounds = false;
    bool bShowCollision = false;
    bool bShowNormals = false;
    bool bShowTangents = false;

    bool bShowStaticMesh = true;
    bool bShowSkeletalMesh = true;
    bool bShowBones = false;
    bool bShowSockets = true;
    bool bShowOnlySelectedBone = false;
    bool bEnableBonePicking = true;
    bool bShowSkinWeights = false;
};
```

규칙:

```text
- Common show flag는 Level Editor와 Previewer가 같은 이름/UX를 사용한다.
- Skeletal 전용 flag는 Skeletal Mesh Previewer에서만 표시한다.
- bShowBones가 false면 bone debug render도, bone picking도 비활성화한다.
- bEnableBonePicking은 bShowBones가 true일 때만 의미가 있다.
- Show menu는 UE5처럼 Viewport toolbar의 "Show" dropdown 아래로 모은다.
```

### Bone Picking

Skeletal Mesh Previewer에서 `Bones` show flag가 켜져 있으면 bone을 선택할 수 있어야 한다.

기본 방향은 ID Picking 기반이다.

```text
Skeletal Mesh Previewer
  -> Show > Bones enabled
  -> bone debug line render
  -> bone pick proxy render into ID buffer
  -> click
  -> PickResult.Kind = Bone
  -> PickResult.BoneIndex
  -> Viewer.SelectedBoneIndex 갱신
  -> Bone details / gizmo target 갱신
```

필요한 구조:

```cpp
enum class EEditorPickTargetKind
{
    Actor,
    Component,
    Bone,
    Socket,
};

struct FEditorPickResult
{
    EEditorPickTargetKind Kind = EEditorPickTargetKind::Actor;
    AActor* Actor = nullptr;
    UActorComponent* Component = nullptr;
    int32 BoneIndex = -1;
    int32 SocketIndex = -1;
};
```

현재 ID Picking이 object id만 담는 구조라면 pick proxy side table을 둔다.

```text
PickId
  -> Viewer id
  -> Component UUID
  -> BoneIndex
```

중요 규칙:

```text
- Bone picking은 Previewer viewport에서만 우선 적용한다.
- Level Editor viewport의 actor/component picking과 섞지 않는다.
- Bones show flag가 꺼져 있으면 bone pick proxy를 ID buffer에 그리지 않는다.
- Bone line이 너무 얇으면 picking용 proxy는 화면상 두께를 별도로 키운다.
- Bone 선택 후 기존 FBoneTransformProxy / Gizmo 연결 흐름을 재사용한다.
```

UE5식 UX 기준:

```text
- 눈에 보이는 debug element만 선택 가능하게 느껴져야 한다.
- Show에서 Bones를 끄면 선택도 꺼진다.
- 선택된 bone은 tree, viewport highlight, details panel이 동기화되어야 한다.
- socket 선택과 bone 선택은 상호 배타적으로 유지한다.
```

---

## 12. Batching Plan

### Batch 0 - Document / Current Structure Freeze

Progress: 0% -> 10%

Tasks:

```text
- Current window/widget structure 기록
- Target tab model 정리
- Command Context / Menu Context / Toolbar Context 정리
- Focus model 정리
```

Output:

```text
- This document
```

### Batch 1 - Command Context Skeleton

Progress: 10% -> 20%

Tasks:

```text
- FEditorCommandId 추가
- FEditorShortcut 추가
- FEditorCommandList 추가
- Active tab command lookup 구조 추가
- Ctrl+S를 ActiveTab Save로 라우팅할 수 있는 뼈대 추가
```

Why first:

```text
탭 UI보다 Command Context가 먼저 있어야 한다.
그렇지 않으면 탭은 생겨도 Menu/Toolbar/Shortcut은 계속 Level Editor 기준으로 남는다.
```

Expected files:

```text
NipsEngine/Source/Editor/UI/EditorCommandContext.h
NipsEngine/Source/Editor/UI/EditorCommandContext.cpp
NipsEngine/Source/Editor/UI/EditorMainPanelInput.cpp
NipsEngine/NipsEngine.vcxproj
NipsEngine/NipsEngine.vcxproj.filters
```

### Batch 2 - Tab Manager Skeleton

Progress: 20% -> 32%

Tasks:

```text
- FEditorTabId 추가
- IEditorTab 추가
- FEditorTabManager 추가
- LevelEditor fixed tab 등록
- Tab strip UI 추가
```

주의:

```text
기존 기능을 아직 크게 이동하지 않는다.
생명주기와 active tab 개념을 먼저 세운다.
```

Expected files:

```text
NipsEngine/Source/Editor/UI/EditorTabManager.h
NipsEngine/Source/Editor/UI/EditorTabManager.cpp
NipsEngine/Source/Editor/UI/EditorMainPanel.h
NipsEngine/Source/Editor/UI/EditorMainPanelFrame.cpp
```

### Batch 3 - Level Editor Fixed Tab

Progress: 32% -> 45%

Tasks:

```text
- 기존 Level Editor viewport host를 LevelEditorTab으로 감싼다.
- LevelEditorTab은 close 불가.
- Level Editor command context 등록.
- Ctrl+S = Save Scene.
- Toolbar = PIE Bar + viewport tools.
```

Must preserve:

```text
- PIE
- picking
- gizmo
- viewport toolbar
- content browser drop
- details / outliner sync
```

### Batch 4 - Contextual Menu / Toolbar

Progress: 45% -> 56%

Tasks:

```text
- Main menu를 Global + ActiveTab menu로 분리.
- Toolbar를 Global + ActiveTab toolbar로 분리.
- PIE Bar를 LevelEditorTab toolbar로 이동.
- Previewer active일 때는 Save Asset 중심 toolbar 표시.
- Material active일 때는 Save/Apply/Revert 중심 toolbar 표시.
```

중요:

```text
이 단계 이후부터 사용자는 탭이 바뀔 때 상단 UI가 바뀌는 것을 체감해야 한다.
```

Additional toolbar requirements:

```text
- Viewport toolbar drawing code는 Level Editor 전용 함수에 계속 묶어두지 않는다.
- DrawViewportIconButton / dropdown / Show menu drawing을 공통 helper로 추출한다.
- Active tab은 자신이 필요한 toolbar item만 builder에 등록한다.
- PIE Bar는 Level Editor toolbar에서만 보인다.
- Previewer active 상태에서는 Level Editor의 transform/pie button이 보이지 않는다.
```

### Batch 5 - Viewer Asset Tabs

Progress: 56% -> 70%

Tasks:

```text
- FEditorViewerWindowWidget을 ViewerTab adapter로 감싼다.
- CreateViewer()는 OpenOrFocusTab(asset path)를 사용한다.
- Static/Skeletal asset type에 따라 title/icon/kind 결정.
- Ctrl+S = Save mesh asset.
- Viewer toolbar = Save / Show / Reset Camera / Reset Pose.
- Close tab 시 Preview World 정리.
```

주의:

```text
Viewer는 이미 Preview World가 있으므로 가장 먼저 tab화하기 좋다.
```

Additional Previewer requirements:

```text
- Viewer viewport toolbar는 Level Editor viewport toolbar와 같은 helper/style을 사용한다.
- Previewer toolbar는 Save / Show / View Mode / Reset Camera / Reset Pose를 기본으로 둔다.
- Static Mesh Previewer는 Bounds / Collision / Normals / Tangents / Material Slot visibility를 Show menu에 둔다.
- Skeletal Mesh Previewer는 Mesh / Bones / Sockets / Bounds / Skin Weight Debug / Bone Picking을 Show menu에 둔다.
- Common show flag와 Static/Skeletal 전용 show flag를 분리한다.
- Show > Bones가 꺼져 있으면 bone debug render와 bone picking을 모두 끈다.
- Show > Bones가 켜지고 Bone Picking이 켜져 있으면 ID Picking으로 bone selection을 지원한다.
- Bone pick result는 SelectedBoneIndex, skeleton tree, details panel, viewport highlight, gizmo proxy와 동기화한다.
```

### Batch 6 - Material Editor Tabs

Progress: 70% -> 82%

Tasks:

```text
- Material Editor를 tab instance로 분리.
- Asset mode / Slot mode TabId 분리.
- Ctrl+S = Save Material.
- Toolbar = Save / Apply / Revert / Preview Mesh.
- Component slot target 삭제 시 dangling 방지.
```

주의:

```text
현재 MaterialWidget은 singleton state에 가깝다.
탭별 상태 분리가 필요해서 Viewer보다 작업량이 크다.
```

### Batch 7 - Curve / Sequencer Tabs

Progress: 82% -> 92%

Tasks:

```text
- ActorSequencerTab 추가.
- CurveEditorTab 추가.
- Sequencer track double click -> CurveEditorTab open/focus.
- Key selection sync 유지.
- Ctrl+S = owning scene or owning curve save.
- Sequencer toolbar = Add Track / Key / Play / Pause / Stop.
```

주의:

```text
Curve와 Sequencer는 서로 상태 sync가 있어서 raw pointer보다 UUID / TrackPath / KeyIndex 중심으로 묶어야 한다.
```

### Batch 8 - Focus / Shortcut / Input Cleanup

Progress: 92% -> 98%

Tasks:

```text
- FEditorFocusState 추가.
- Active tab / focused viewport / hovered viewport 명시화.
- Modal / text input / popup 상태에서 viewport input 차단.
- Preview viewport와 Level viewport input 경계 정리.
- ShortcutRouter 최종 정리.
```

### Batch 9 - Legacy Cleanup / Verification

Progress: 98% -> 100%

Tasks:

```text
- 대체된 bShowMaterialEditor 등 visibility 제거.
- OpenViewer / CloseViewer / OpenMaterialAsset API 정리.
- 기존 PIE Bar가 전역처럼 남아 있는 부분 제거.
- 빌드 확인.
- 실제 editor UX 시나리오 검증.
```

Verification:

```text
- Level Editor tab은 항상 존재하고 닫히지 않는다.
- Level Editor active에서 Ctrl+S는 scene 저장.
- Previewer active에서 Ctrl+S는 mesh asset 저장.
- Material active에서 Ctrl+S는 material 저장.
- Level Editor active에서만 PIE Bar가 보인다.
- Previewer active에서는 Save / Show / Reset 계열 toolbar만 보인다.
- 같은 mesh asset을 두 번 열면 기존 tab focus.
- Actor 삭제 후 Material slot tab / Sequencer tab dangling 없음.
- PIE possessed input은 PlayerController로만 전달.
- Previewer viewport input은 Preview World에만 적용.
```

Additional verification for Previewer:

```text
- Skeletal Previewer에서 Show > Bones를 켜면 bone debug line이 보인다.
- Bones가 켜진 상태에서 bone을 클릭하면 ID Picking으로 해당 bone이 선택된다.
- Show > Bones를 끄면 bone picking도 비활성화된다.
- Bone 선택은 skeleton tree, details panel, gizmo proxy와 동기화된다.
```

---

## 13. Scope Estimate

보수적으로는 중간 이상 규모다.

```text
Command Context skeleton: small ~ medium
Tab Manager skeleton: small ~ medium
Level Editor fixed tab: medium
Contextual Menu/Toolbar: medium
Viewer tabs: medium
Material tabs: medium ~ large
Curve/Sequencer tabs: medium ~ large
Focus/Input cleanup: large
Legacy cleanup: medium
```

추천 1차 완료 기준:

```text
- Command Context skeleton
- Tab Manager skeleton
- Level Editor fixed tab
- Contextual toolbar/menu 기본 동작
- Viewer asset tab화
```

2차 완료 기준:

```text
- Material Editor tab화
- Curve / Actor Sequencer tab화
- Focus / Shortcut 완성도 향상
```

3차 완료 기준:

```text
- Layout 저장/복원
- Detached/floating tab
- Multi asset editor session restore
```

---

## 14. Wrapping Strategy

이번 리팩토링은 rewrite가 아니라 wrapping이다.

기존 구조를 최대한 존중하면서 다음 레이어만 얹는다.

```text
기존 Widget / Viewer / Editor 기능
  -> Tab Adapter
      -> Command Context
      -> Menu Context
      -> Toolbar Context
      -> Focus Context
```

즉, `FEditorViewer`, `FEditorMaterialWidget`, `FEditorActorSequencerWidget`, `FEditorCurveEditorWidget`를 바로 갈아엎지 않는다.
먼저 tab adapter가 기존 객체를 감싸고, 필요할 때만 내부 상태를 분리한다.

### Adapter-first 원칙

```cpp
class FViewerTab : public IEditorTab
{
public:
    FViewerTab(FEditorViewer* InViewer);

    FEditorTabId GetTabId() const override;
    FString GetTitle() const override;
    EEditorTabKind GetKind() const override;

    void BuildToolbar(FEditorToolbarBuilder& Toolbar) override;
    void RegisterCommands(FEditorCommandList& Commands) override;
    bool Save() override;
    void Render(float DeltaTime) override;

private:
    FEditorViewer* Viewer = nullptr;
};
```

처음에는 기존 `FEditorViewerWindowWidget`의 기능을 최대한 재사용한다.
다만 장기적으로는 ImGui window begin/end와 실제 content rendering을 분리해야 한다.

```text
Before
  FEditorViewerWindowWidget::Render()
    -> ImGui::Begin("Viewer")
    -> Render skeleton tree
    -> Render viewport
    -> Render details
    -> ImGui::End()

After
  FViewerTab::Render()
    -> Tab body 영역에서 RenderViewerContent()

  FEditorViewerContent
    -> Render skeleton tree
    -> Render viewport
    -> Render details
```

이렇게 하면 기존 기능은 유지하면서도, 나중에 floating window / docked tab / split view를 같은 content로 재사용할 수 있다.

### Level Editor Wrapping

Level Editor는 새로 만들지 않는다.

현재 `FEditorMainPanel`이 들고 있는 다음 기능을 `FLevelEditorTab`에서 감싸는 형태로 시작한다.

```text
RenderViewportHostWindow()
RenderViewportOverlayWidget()
SceneWidget / PropertyWidget / ControlWidget
ContentBrowser
PIE controls
Viewport toolbar
```

초기에는 `FLevelEditorTab`이 MainPanel의 private 기능을 바로 다 가져가기 어렵다.
따라서 1차에서는 MainPanel 안에서 LevelEditorTab 개념을 세우고, 실제 함수 이동은 천천히 한다.

권장 진행:

```text
1. MainPanel에 TabManager를 추가한다.
2. LevelEditorTab은 close 불가 active tab으로만 먼저 존재한다.
3. 기존 RenderMainViewport(), RenderEditorPanelWindows() 흐름은 유지한다.
4. Toolbar/Menu/Shortcut만 active tab context를 타도록 바꾼다.
5. 이후 Level Editor content를 별도 class로 천천히 추출한다.
```

### Material Editor Wrapping

Material Editor는 현재 singleton state에 가깝다.
따라서 한 번에 multi-tab material editor로 바꾸면 위험하다.

1차 wrapping:

```text
FMaterialEditorTab
  -> 기존 FEditorMaterialWidget을 감싼다.
  -> active material target만 현재처럼 하나 유지한다.
  -> Ctrl+S / toolbar / menu context를 먼저 연결한다.
```

2차 분리:

```text
FMaterialEditorTabState
  - EditingSlotIndex
  - SelectedMaterialPtr
  - EditingSlotOwner
  - AssetEditingMaterialPtr
  - PreviewMesh
  - PreviewCamera state
```

이 상태를 tab instance별로 들게 하면 같은 시점에 여러 material tab을 안전하게 열 수 있다.

### Curve / Sequencer Wrapping

Curve Editor와 Actor Sequencer는 서로 selection sync가 있으므로 content 분리보다 model 분리가 먼저다.

```text
FActorSequencerTab
  -> 기존 FEditorActorSequencerWidget 감싸기

FCurveEditorTab
  -> 기존 FEditorCurveEditorWidget 감싸기

공유 데이터
  -> SequenceComponent UUID
  -> TrackPath
  -> SelectedKeyIndex
```

즉, raw pointer를 탭의 정체성으로 삼지 않고, stable id로 다시 검증하는 흐름을 둔다.

### Why wrapping first

```text
- 기존 기능 보존이 쉽다.
- 한 번에 UI를 전부 갈아엎지 않아도 된다.
- build 가능한 상태를 batch마다 유지할 수 있다.
- Viewer / Material / Sequencer의 위험도가 서로 다르므로 순차 이전이 가능하다.
- 나중에 필요하면 content renderer만 분리해서 더 깔끔한 구조로 발전시킬 수 있다.
```

---

## 15. Implementation Principles

```text
- Level Editor는 닫지 않는다.
- Asset editor는 asset path 기준으로 중복 오픈을 막는다.
- Tool tab은 target UUID 기준으로 중복 오픈을 막는다.
- 리팩토링은 rewrite가 아니라 adapter-first wrapping으로 시작한다.
- 기존 기능은 content renderer로 천천히 추출한다.
- Ctrl+S는 active tab의 save 의미를 따른다.
- PIE Bar는 Level Editor의 toolbar다.
- Previewer는 Previewer 전용 toolbar와 shortcut을 가진다.
- Level World와 Preview World를 섞지 않는다.
- Raw pointer를 장기 저장할 때는 actor/component destruction을 반드시 고려한다.
- Active tab, focused viewport, runtime input owner를 분리해서 본다.
- 대체 구조가 생기면 Legacy API를 오래 살리지 않는다.
```
