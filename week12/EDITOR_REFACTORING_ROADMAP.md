# Editor Refactoring Roadmap

## 1. 목적

이번 리팩토링의 목적은 기능을 더 많이 추가하는 것이 아니라, 이미 커진 Editor 코드의 책임을 다시 나누는 것이다.

현재 Editor 쪽은 Widget이 UI 표시뿐 아니라 Scene 저장/로드, Asset 조회, Command 실행, Footer Log 출력, ResourceManager 접근까지 직접 처리하는 경우가 많다.

이 구조는 작은 기능을 빠르게 붙일 때는 편하지만, 기능이 늘어나면 다음 문제가 생긴다.

- Widget이 너무 많은 엔진/에디터 내부 구조를 알게 된다.
- 같은 기능을 Toolbar, Shortcut, ContentBrowser, Menu에서 각각 다르게 호출하게 된다.
- ResourceManager, MainPanel, SceneWidget 같은 구체 타입 의존이 UI 곳곳으로 퍼진다.
- 팀원이 새 기능을 붙일 때 어디에 넣어야 하는지 애매해진다.

따라서 이번 리팩토링의 큰 방향은 다음과 같다.

```text
Widget은 UI를 그린다.
Service는 Editor 기능을 수행한다.
CommandSystem은 사용자의 명령을 한 곳에서 실행한다.
Engine Runtime 기능과 Editor 편의 기능을 분리한다.
```

---

## 2. 현재 구조

현재 구조는 대략 다음과 같다.

```text
EditorMainPanel
  -> ToolbarWidget
  -> SceneWidget
  -> ContentBrowserWidget
  -> PropertyWidget
  -> MaterialWidget
  -> FooterLog

각 Widget
  -> EditorEngine
  -> MainPanel
  -> ResourceManager
  -> SceneSaveManager
  -> ProjectSettings
  -> UndoSystem
```

문제는 Widget이 UI 계층인데도 실제 기능 실행까지 많이 담당한다는 점이다.

예를 들어 기존 Scene 관련 구조는 다음과 비슷했다.

```text
ToolbarWidget
  -> SceneWidget.NewScene()
  -> SceneWidget.SaveScene()

ContentBrowserWidget
  -> SceneWidget.LoadSceneFromFilePath()

Packaging
  -> SceneWidget.PromptSaveIfDirty()

Footer
  -> SceneWidget.GetCurrentSceneDisplayPath()
```

이 구조에서는 Scene 기능이 필요할 때 SceneWidget을 알아야 한다. 하지만 SceneWidget은 원래 Outliner UI에 가깝다. 그래서 Scene 저장/로드 기능이 Widget에 묶이는 문제가 있었다.

이 문제는 1차로 `FEditorSceneService`를 추가하면서 정리했다.

```text
Before
  Widget -> SceneWidget -> Scene 기능

After
  Widget -> EditorEngine.GetSceneService() -> Scene 기능
```

이제 같은 방식으로 Property, Asset, Command, Notification 쪽도 정리한다.

---

## 3. 최종 목표 구조

최종 목표는 다음 구조다.

```text
UEditorEngine
  -> FEditorSceneService
  -> FEditorAssetService
  -> FEditorCommandSystem
  -> FEditorNotificationService
  -> FEditorUndoSystem
  -> FSelectionManager

Editor Widgets
  -> UI 표시
  -> 사용자 입력을 Service/Command로 전달

Engine Runtime
  -> 순수 Runtime 기능
  -> GameJam/Test/EditorPreview 전용 코드와 분리
```

의존 방향은 다음처럼 잡는다.

```text
Widget
  -> EditorEngine
  -> Editor Service
  -> Engine API

Widget
  X ResourceManager 직접 호출 최소화
  X MainPanel을 통한 기능 호출 최소화
  X 다른 Widget의 내부 API 직접 호출 금지
```

핵심 규칙은 이렇다.

```text
Widget이 다른 Widget에게 일을 시키지 않는다.
Widget은 Service 또는 CommandSystem에게 일을 요청한다.
Service는 필요한 Engine API를 호출한다.
```

---

## 4. 리팩토링 대상

이번 문서에서 다루는 대상은 다음 5개다.

```text
1. EditorPropertyWidget 분리
2. EditorAssetService 추가
3. EditorCommandSystem 추가
4. ContentBrowser Action 분리
5. Editor Notification/Footer Log 분리
```

Renderer 쪽은 다른 팀원이 담당하므로, 여기서는 RenderPass, Renderer, Shader, RenderResource 구조 변경은 제외한다.

---

## 5. EditorPropertyWidget 분리

### 현재 문제

`FEditorPropertyWidget`은 현재 Details Panel 역할을 하면서도 너무 많은 기능을 알고 있다.

예상 책임은 다음처럼 섞여 있다.

```text
FEditorPropertyWidget
  -> Actor Transform 표시/수정
  -> Component 공통 속성 표시/수정
  -> StaticMesh 선택 UI
  -> SkeletalMesh 선택 UI
  -> Material Slot UI
  -> Texture/Material 목록 조회
  -> Script 생성/연결
  -> Skeletal Bone Debug UI
  -> Scene Dirty 처리
  -> Footer Log 출력
```

이 구조는 Component 종류가 늘어날수록 파일이 계속 커진다.

### 목표 구조

PropertyWidget은 선택된 객체를 보고 적절한 Details Renderer를 호출하는 역할만 한다.

```text
FEditorPropertyWidget
  -> Selection 읽기
  -> Details Section 호출

Details Section
  -> FActorTransformDetails
  -> FSceneComponentDetails
  -> FPrimitiveComponentDetails
  -> FStaticMeshComponentDetails
  -> FSkeletalMeshComponentDetails
  -> FMaterialSlotDetails
  -> FScriptComponentDetails
```

예상 API는 다음과 같다.

```cpp
struct FEditorDetailsContext
{
    UEditorEngine* EditorEngine = nullptr;
    FEditorSceneService* SceneService = nullptr;
    FEditorAssetService* AssetService = nullptr;
    FEditorNotificationService* NotificationService = nullptr;
};

class IEditorDetailsSection
{
public:
    virtual ~IEditorDetailsSection() = default;
    virtual bool CanDraw(UObject* Target) const = 0;
    virtual void Draw(UObject* Target, const FEditorDetailsContext& Context) = 0;
};
```

초기에는 꼭 interface까지 만들 필요는 없다. 파일만 나누고 static helper 함수로 시작해도 된다.

```cpp
namespace FStaticMeshComponentDetails
{
    void Draw(UStaticMeshComponent* Component, const FEditorDetailsContext& Context);
}
```

### 기대 효과

- PropertyWidget 파일 크기가 줄어든다.
- StaticMesh, SkeletalMesh, Script, Material 작업이 서로 덜 충돌한다.
- 새 Component Details를 추가할 위치가 명확해진다.
- SkeletalMeshComponent 전용 UI를 분리해서 이번 Skeleton 작업과도 연결하기 쉽다.

---

## 6. EditorAssetService 추가

### 현재 문제

Editor UI 여러 곳에서 `FResourceManager::Get()`을 직접 호출한다.

예시 책임은 다음과 같다.

```text
PropertyWidget
  -> StaticMesh 목록 조회
  -> SkeletalMesh 목록 조회
  -> Material 목록 조회
  -> Texture 목록 조회

MaterialWidget
  -> Material Instance 생성
  -> Material Serialize
  -> Texture 목록 조회

ToolbarWidget
  -> Asset Directory Refresh

ContentBrowserWidget
  -> Asset 생성/열기
```

`ResourceManager`는 Runtime Resource를 관리하는 쪽에 가깝고, Editor UI가 원하는 것은 "에디터에서 보여줄 Asset 목록과 작업"이다.

### 목표 구조

Editor 전용 Asset 접근은 `FEditorAssetService`로 모은다.

```text
Widget
  -> FEditorAssetService
  -> FResourceManager
```

예상 API는 다음과 같다.

```cpp
class FEditorAssetService
{
public:
    void Initialize(UEditorEngine* InEditorEngine);

    const TArray<FString>& GetStaticMeshAssetPaths() const;
    const TArray<FString>& GetSkeletalMeshAssetPaths() const;
    const TArray<FString>& GetTextureAssetPaths() const;
    const TArray<FString>& GetMaterialNames() const;
    const TArray<FString>& GetFontNames() const;
    const TArray<FString>& GetParticleNames() const;

    UStaticMesh* LoadStaticMesh(const FString& Path);
    USkeletalMesh* LoadSkeletalMesh(const FString& Path);
    UTexture* LoadTexture(const FString& Path);
    UMaterialInterface* GetMaterialInterface(const FString& NameOrPath);

    UMaterialInstance* CreateMaterialInstance(const FString& InstancePath, UMaterial* Parent);
    bool SaveMaterialInstance(UMaterialInstance* Instance);

    void RefreshAssetDatabase();
};
```

### 중요한 기준

`FEditorAssetService`가 ResourceManager를 완전히 대체하는 것은 아니다.

```text
ResourceManager
  -> Runtime Resource Load/Cache 담당

EditorAssetService
  -> Editor UI에서 필요한 Asset 목록, 선택, 생성, 갱신 흐름 담당
```

### 성능 목표

`FEditorAssetService`는 Property 창의 드롭다운 프리즈를 줄이기 위한 성능 경계도 담당한다.

현재 Property UI에서는 StaticMesh, SkeletalMesh, Texture, Material 목록을 그리는 시점에 ResourceManager를 직접 호출한다. 이 경우 프리즈가 발생했을 때 원인이 다음 중 무엇인지 구분하기 어렵다.

```text
Asset 파일 목록 스캔이 느린 것인지
ResourceManager 내부 캐시 조회가 느린 것인지
실제 Mesh/Material 로딩이 발생한 것인지
ImGui 드롭다운에 너무 많은 항목을 그려서 느린 것인지
```

따라서 AssetService는 다음 원칙을 가진다.

```text
목록 조회는 가벼운 Asset Catalog 조회만 수행한다.
드롭다운을 여는 것만으로 Mesh/Material을 실제 로딩하지 않는다.
실제 리소스 로딩은 사용자가 항목을 선택했을 때만 수행한다.
Asset Catalog는 Editor 시작 시점 또는 Refresh 시점에 미리 갱신한다.
목록 갱신, 리소스 로딩, UI 렌더링 시간을 각각 따로 측정할 수 있게 한다.
```

권장 데이터 구조는 다음과 같다.

```cpp
enum class EEditorAssetType
{
    StaticMesh,
    SkeletalMesh,
    Texture,
    Material,
    Font,
    Particle,
    Scene,
    Script,
};

struct FEditorAssetItem
{
    FString Path;
    FString DisplayName;
    EEditorAssetType Type;
};
```

PropertyWidget은 다음처럼 실제 리소스 목록 대신 AssetService의 Catalog를 사용한다.

```cpp
const TArray<FEditorAssetItem>& MeshAssets =
    EditorEngine->GetAssetService().GetAssets(EEditorAssetType::StaticMesh);
```

이렇게 하면 "드롭다운 구성"과 "리소스 로딩"이 분리된다.

프리즈가 남아 있다면 다음처럼 원인을 나눠 볼 수 있다.

```text
Catalog Refresh가 느림
  -> 파일 시스템 스캔 문제

Dropdown Render가 느림
  -> 검색/필터/ImGuiListClipper 필요

Load Selected Asset이 느림
  -> 실제 Mesh Import/Load 문제
```

### 기대 효과

- Widget에서 ResourceManager 직접 호출이 줄어든다.
- Asset 목록 조회 방식이 한 곳에 모인다.
- MaterialWidget, PropertyWidget, ContentBrowser가 같은 API를 사용한다.
- 나중에 Reimport, Asset Metadata, Thumbnail, Favorite 같은 기능을 붙이기 쉬워진다.

---

## 7. EditorCommandSystem 추가

### 현재 문제

같은 기능이 여러 입력 경로에서 호출될 수 있다.

```text
New Scene
  -> Toolbar Menu
  -> Shortcut
  -> Main Menu

Save Scene
  -> Toolbar Menu
  -> Shortcut
  -> Packaging 전 Prompt

Play/Stop
  -> Toolbar Button
  -> Shortcut
  -> Viewport Shortcut
```

각 UI가 기능을 직접 호출하면 단축키와 메뉴의 동작이 달라질 수 있다.

### 목표 구조

Editor 명령은 `FEditorCommandSystem`에서 실행한다.

```cpp
enum class EEditorCommand
{
    NewScene,
    OpenScene,
    SaveScene,
    SaveSceneAs,
    Play,
    Stop,
    Undo,
    Redo,
    RefreshAssets,
    BuildGame,
    PackageGame,
};

class FEditorCommandSystem
{
public:
    void Initialize(UEditorEngine* InEditorEngine);
    bool CanExecute(EEditorCommand Command) const;
    bool Execute(EEditorCommand Command);
};
```

CommandSystem의 목적은 실제 기능을 새로 구현하는 것이 아니다.

```text
Toolbar
Shortcut
Viewport Hotkey
ContentBrowser ContextMenu
Main Menu
```

이 입력 경로들이 같은 Editor 기능을 각자 직접 호출하지 않게 하는 실행 관문이다.

예를 들어 기존에는 `SaveScene`이 Toolbar shortcut, File menu, Packaging 전 처리 등 여러 곳에서 직접 호출될 수 있다. CommandSystem을 두면 UI는 "SaveScene 명령 실행"만 요청하고, 실제로 어떤 Service를 호출할지는 CommandSystem이 결정한다.

```text
Before
  Toolbar -> SceneService.SaveScene()
  Shortcut -> SceneService.SaveScene()
  Menu -> SceneService.SaveScene()

After
  Toolbar -> CommandSystem.Execute(SaveScene)
  Shortcut -> CommandSystem.Execute(SaveScene)
  Menu -> CommandSystem.Execute(SaveScene)
             -> SceneService.SaveScene()
```

중요한 점은 CommandSystem이 Dialog UI를 소유하지 않는다는 것이다.

```text
Open File Dialog
Save File Dialog
Popup / Modal
```

이런 UI 선택은 Widget이 처리하고, 선택된 결과만 CommandSystem에 넘긴다.

```cpp
FEditorCommandArgs Args;
Args.ScenePath = PickedPath;
EditorEngine->GetCommandSystem().Execute(EEditorCommand::OpenScene, Args);
```

사용 예시는 다음과 같다.

```cpp
EditorEngine->GetCommandSystem().Execute(EEditorCommand::SaveScene);
```

Toolbar, Shortcut, Menu는 모두 같은 Command를 호출한다.

### 기대 효과

- UI 경로별 동작 차이가 줄어든다.
- Shortcut 추가가 쉬워진다.
- Command별 CanExecute 조건을 한 곳에서 관리할 수 있다.
- ToolbarWidget이 SceneService, AssetService, PackagingService 등을 직접 알 필요가 줄어든다.

### Undo System과의 관계

`EditorCommandSystem`을 추가한다고 해서 모든 Command를 곧바로 Undo Command로 만들 필요는 없다.

초기에는 다음처럼 역할을 분리한다.

```text
EditorCommand
  -> 사용자의 명령을 실행하는 라우팅 단위
  -> Toolbar / Shortcut / Menu / ContextMenu가 공유

UndoTransaction
  -> 되돌릴 수 있는 편집 작업의 기록 단위
  -> Actor 이동, Component 추가, Material Slot 변경 같은 Scene 변경을 기록
```

즉 `SaveScene`, `OpenScene`, `Play`, `RefreshAssets` 같은 명령은 EditorCommand지만 Undo 대상은 아니다.

반대로 `MoveActor`, `AddComponent`, `DeleteActor`, `EditProperty` 같은 명령은 EditorCommand이면서 UndoTransaction을 만들 수 있다.

현재 Undo System은 전체 Scene을 문자열 snapshot으로 저장한다.

```text
FEditorUndoSystem
  -> UEditorEngine.CaptureSceneSnapshot()
  -> FSceneSaveManager.SaveToString()
  -> Undo 시 전체 Scene Restore
```

이 방식은 단일 Editor World에서는 단순하고 강하다. 하지만 Viewer, Preview World, PIE World처럼 동시에 유지하는 World가 늘어나면 문제가 생길 수 있다.

```text
Undo History가 어느 World에 대한 것인지 불명확해진다.
Viewer World 변경까지 Main Editor Scene Undo에 섞일 수 있다.
전체 World snapshot은 무겁다.
작은 Property 변경도 전체 Scene 복원으로 처리된다.
```

따라서 CommandSystem을 도입할 때 Undo까지 고려한다면 최종 목표는 다음 구조다.

```text
FEditorCommandSystem
  -> 명령 실행
  -> 필요한 경우 UndoSystem.BeginTransaction()
  -> 실제 Service 호출
  -> UndoSystem.EndTransaction()

FEditorUndoSystem
  -> World별 Undo Stack 관리
  -> 초기에는 Snapshot Transaction 유지
  -> 이후 Object/Property 단위 Transaction으로 확장
```

권장 API 초안은 다음과 같다.

```cpp
struct FEditorUndoContext
{
    FName WorldHandle;
    FString Label;
};

class FEditorUndoSystem
{
public:
    bool CaptureSnapshot(const FEditorUndoContext& Context);
    bool Undo(FName WorldHandle);
    bool Redo(FName WorldHandle);
    void ClearHistory(FName WorldHandle);
};
```

더 장기적으로는 Snapshot 기반과 Command 기반을 같이 둘 수 있다.

```cpp
class IEditorUndoTransaction
{
public:
    virtual ~IEditorUndoTransaction() = default;
    virtual const FString& GetLabel() const = 0;
    virtual FName GetWorldHandle() const = 0;
    virtual bool Undo() = 0;
    virtual bool Redo() = 0;
};
```

초기 마이그레이션에서는 곧바로 모든 Undo를 Command Pattern으로 바꾸지 않는다.

권장 단계는 다음과 같다.

```text
Step 1. Undo Snapshot에 WorldHandle을 붙인다.
Step 2. Undo/Redo 호출 시 Active Editor World의 History만 사용한다.
Step 3. EditorCommandSystem에서 Undo 가능한 명령만 CaptureSnapshot을 감싼다.
Step 4. 빈번한 Property Edit부터 Object/Property Transaction으로 교체한다.
Step 5. 전체 Scene Snapshot은 큰 작업 또는 fallback으로 유지한다.
```

이렇게 하면 현재 안정적인 전체 Snapshot Undo를 바로 버리지 않으면서도, 여러 World가 생기는 구조에 대응할 수 있다.

---

## 8. ContentBrowser Action 분리

### 현재 문제

ContentBrowser는 파일/폴더 UI를 표시하는 역할이지만, 실제 Asset 동작까지 직접 처리하기 쉽다.

예상 책임은 다음처럼 섞일 수 있다.

```text
ContentBrowserWidget
  -> Directory 표시
  -> File 선택
  -> DoubleClick 처리
  -> Scene 열기
  -> Material 열기
  -> Asset 생성
  -> Rename/Delete
  -> Import/Reimport
```

이러면 ContentBrowser가 Scene, Material, Asset, ResourceManager를 모두 알게 된다.

### 목표 구조

파일 UI와 파일 액션을 분리한다.

```text
FEditorContentBrowserWidget
  -> 파일/폴더 표시
  -> 선택/더블클릭 이벤트 전달

FContentBrowserActionService
  -> 확장자별 동작 처리
  -> SceneService / AssetService / MaterialEditor 호출
```

예상 API는 다음과 같다.

```cpp
class FContentBrowserActionService
{
public:
    void Initialize(UEditorEngine* InEditorEngine);

    bool OpenAsset(const FString& AssetPath);
    bool CreateAsset(const FString& DirectoryPath, const FString& AssetType);
    bool RenameAsset(const FString& OldPath, const FString& NewPath);
    bool DeleteAsset(const FString& AssetPath);
};
```

확장자별 동작 예시는 다음과 같다.

```text
.scene
  -> SceneService.OpenScene()

.mat / .matinst
  -> Material Editor 열기

.fbx / .obj
  -> Mesh Preview 또는 Import/Reimport

.lua
  -> Script Editor 또는 외부 편집기
```

### 기대 효과

- ContentBrowserWidget이 UI에 집중한다.
- Asset 동작 정책을 한 곳에서 바꿀 수 있다.
- 새로운 Asset Type을 추가할 때 ContentBrowser 본체를 덜 건드린다.

---

## 9. Editor Notification/Footer Log 분리

### 현재 문제

여러 코드가 직접 MainPanel Footer에 로그를 넣는다.

```cpp
EditorEngine->GetMainPanel().PushFooterLog("Scene saved");
```

이 방식은 간단하지만, MainPanel에 대한 의존이 너무 많이 퍼진다.

또한 나중에 Toast, Modal, Error Dialog, Status Bar를 분리하려면 호출부를 많이 고쳐야 한다.

### 목표 구조

에디터 알림은 `FEditorNotificationService`로 모은다.

```cpp
enum class EEditorNotificationType
{
    Info,
    Warning,
    Error,
};

class FEditorNotificationService
{
public:
    void Initialize(UEditorEngine* InEditorEngine);

    void Info(const FString& Message);
    void Warning(const FString& Message);
    void Error(const FString& Message);
};
```

사용 예시는 다음과 같다.

```cpp
EditorEngine->GetNotificationService().Info("Scene saved");
EditorEngine->GetNotificationService().Warning("No actor selected");
EditorEngine->GetNotificationService().Error("Failed to save material");
```

초기 구현은 내부에서 기존 FooterLog를 호출해도 된다.

```text
NotificationService
  -> MainPanel.PushFooterLog()
```

중요한 것은 호출부가 더 이상 MainPanel을 직접 알지 않는 것이다.

### 기대 효과

- MainPanel 직접 의존이 줄어든다.
- Footer Log, Toast, Modal을 나중에 쉽게 분리할 수 있다.
- 에러/경고/정보 메시지 레벨을 구분할 수 있다.

---

## 10. 권장 진행 순서

추천 순서는 다음과 같다.

```text
Step 1. 현재 SceneService 리팩토링 커밋
Step 2. EditorNotificationService 추가
Step 3. EditorAssetService 추가
Step 4. EditorPropertyWidget Details Section 분리
Step 5. EditorCommandSystem 추가
Step 6. ContentBrowserActionService 추가
```

이 순서를 추천하는 이유는 다음과 같다.

```text
NotificationService
  -> 가장 작고 영향 범위가 명확함

AssetService
  -> PropertyWidget / MaterialWidget 분리 전에 먼저 있으면 좋음

PropertyWidget 분리
  -> 가장 체감이 크지만 파일이 크므로 앞 단계가 있으면 정리하기 쉬움

CommandSystem
  -> SceneService / AssetService가 어느 정도 있어야 깔끔하게 연결됨

ContentBrowserActionService
  -> SceneService / AssetService / CommandSystem을 활용하는 후속 단계
```

---

## 11. 하지 않을 것

이번 리팩토링에서는 다음은 범위에서 제외한다.

```text
Renderer / RenderPass 내부 구조 변경
Shader 구조 변경
RenderResource lifetime 변경
GPU Skinning 구현
Undo System의 전면 Transaction 전환
엔진 전체 UObject/Reflection 구조 변경
대규모 파일 이동
```

Renderer 담당자와 충돌을 피하기 위해 Render 계층은 필요한 호출부만 최소한으로 건드린다.

Undo Transaction 구조는 Deferred로 둔다.

현재 단계에서는 전체 Snapshot Undo를 유지하고, 여러 World 문제가 실제로 커지기 전에 `WorldHandle` 기반 Undo Stack 분리를 먼저 검토한다. Command Pattern 기반 Transaction은 이후 Transform, Material Slot, Component Add/Remove처럼 범위가 명확한 작업부터 점진적으로 도입한다.

---

## 12. 완료 기준

각 단계의 완료 기준은 다음과 같다.

```text
EditorNotificationService
  -> MainPanel.PushFooterLog 직접 호출이 대부분 NotificationService로 이동

EditorAssetService
  -> Editor UI의 ResourceManager 직접 호출이 의미 있는 수준으로 감소

PropertyWidget 분리
  -> StaticMesh / SkeletalMesh / MaterialSlot / Script Details 코드가 별도 파일로 이동

EditorCommandSystem
  -> Toolbar와 Shortcut이 같은 Command API를 사용

ContentBrowserActionService
  -> ContentBrowserWidget이 확장자별 Asset 실행 정책을 직접 알지 않음
```

최종적으로 목표하는 상태는 다음이다.

```text
Editor UI는 얇아지고,
Editor 기능은 Service로 이동하며,
사용자 명령은 CommandSystem으로 통합되고,
Engine Runtime과 Editor 편의 코드의 경계가 명확해진다.
```
