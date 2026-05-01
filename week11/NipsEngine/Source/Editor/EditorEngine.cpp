#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Slate/SlateApplication.h"
#include "Engine/Input/InputSystem.h"
#include "Runtime/ViewportRect.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Core/Logging/Stats.h"
#include "Runtime/Script/ScriptManager.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Settings/EditorSettings.h"
#include <algorithm>
#include <unordered_set>
#include <utility>

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

namespace
{
    bool HasPlayerStart(UWorld* World)
    {
        if (!World)
        {
            return false;
        }

        for (AActor* Actor : World->GetActors())
        {
            if (Actor && Actor->IsA<APlayerStart>())
            {
                return true;
            }
        }
        return false;
    }

    void SpawnDefaultSceneActors(UWorld* World)
    {
        if (!World)
        {
            return;
        }

        ADirectionalLightActor* DirectionalLight = World->SpawnActor<ADirectionalLightActor>();
        if (DirectionalLight)
        {
            DirectionalLight->InitDefaultComponents();
            DirectionalLight->SetFName(FName("Directional Light"));
            DirectionalLight->SetActorLocation(FVector(0.0f, 0.0f, 13.0f));
            DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
        }

        AAmbientLightActor* AmbientLight = World->SpawnActor<AAmbientLightActor>();
        if (AmbientLight)
        {
            AmbientLight->InitDefaultComponents();
            AmbientLight->SetFName(FName("Ambient Light"));
            AmbientLight->SetActorLocation(FVector(0.0f, 0.0f, 15.0f));
        }

        if (!HasPlayerStart(World))
        {
            APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>();
            if (PlayerStart)
            {
                PlayerStart->InitDefaultComponents();
                PlayerStart->SetFName(FName("Player Start"));
                PlayerStart->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
            }
        }

        World->SyncSpatialIndex();
    }
}

//  Init
void UEditorEngine::Init(FWindowsWindow* InWindow)
{
    UEngine::Init(InWindow);
    InputSystem::Get().SetOwnerWindow(Window ? Window->GetHWND() : nullptr);
    EditorInputRouter.SetOwnerWindow(Window ? Window->GetHWND() : nullptr);
    FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

    MainPanel.Create(Window, Renderer, this);
    bool bCreatedStartupWorld = false;
    if (WorldList.empty())
    {
        CreateWorldContext(EWorldType::Editor, FName("Default"));
        bCreatedStartupWorld = true;
    }
    SetActiveWorld(WorldList[0].ContextHandle);
    ApplySpatialIndexMaintenanceSettings();
    if (bCreatedStartupWorld)
    {
        SpawnDefaultSceneActors(WorldList[0].World);
    }

    // Selection & Gizmo
    SelectionManager.Init();
    ViewportLayout.Init(InWindow, GetWorld(), &SelectionManager, this);
    GetFocusedWorld()->SetActiveCamera(GetCamera());

    // Slate 초기화 및 Viewport Layout 추가
    FSlateApplication::Get().Initialize();
    ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));

    // Editor용 렌더 파이프라인 세팅
    SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));

	FScriptManager::Get().initializeLuaState();
}

void UEditorEngine::Shutdown()
{
    // 스플리터 비율을 Settings 에 기록 후 저장
    if (SSplitterV* SV = ViewportLayout.GetRootSplitterV())
        FEditorSettings::Get().SplitterVRatio = SV->GetSplitRatio();
    if (SSplitterH* SH = ViewportLayout.GetTopSplitterH())
        FEditorSettings::Get().SplitterHRatio = SH->GetSplitRatio();

    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());

    CloseScene();
    SelectionManager.Shutdown();
    MainPanel.Release();
    
    // CloseScene 이후에 ViewportLayout을 내리면 Client 포인터 단절로 인한 역참조를 피할 수 있습니다.
    ViewportLayout.Shutdown();           // 위젯 트리 해제 (소유권: UEditorEngine)
    FSlateApplication::Get().Shutdown(); // RootWindow 해제

    // 엔진 공통 해제 (Renderer, D3D 등)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    ViewportLayout.OnWindowResized(Width, Height);
}


void UEditorEngine::Tick(float DeltaTime)
{
    ProcessQueuedPlaySessionRequests();

    const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
    const bool bGuiKeyboardCaptureForViewport =
        (GuiState.bUsingKeyboard || GuiState.bUsingTextInput) && !GuiState.bAllowViewportMouseFocus;
    EditorInputRouter.SetImGuiCaptureState(GuiState.bUsingMouse, bGuiKeyboardCaptureForViewport);
    EditorInputRouter.SetForceViewportMouseBlock(GuiState.bBlockViewportMouse, GuiState.bAllowViewportMouseFocus);
    RegisterViewportInputTargets();
    FViewportInputContext RoutedInputContext;
    FInteractionBinding RoutedInputBinding;
    EditorInputRouter.Tick(DeltaTime, RoutedInputContext, RoutedInputBinding);
    for (int32 Index = 0; Index < FEditorViewportLayout::MaxViewports; ++Index)
    {
        if (FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(Index))
        {
            ViewportClient->SetLegacyInputSuppressedThisFrame(true);
        }
    }

    ViewportLayout.Tick(DeltaTime);
    MainPanel.Update();
    WorldTick(DeltaTime);
    Render(DeltaTime);
}

void UEditorEngine::RegisterViewportInputTargets()
{
    EditorInputRouter.ClearTargets();

    for (int32 Index = 0; Index < FEditorViewportLayout::MaxViewports; ++Index)
    {
        FSceneViewport& SceneViewport = ViewportLayout.GetSceneViewport(Index);
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(Index);
        if (!ViewportClient)
        {
            continue;
        }

        EditorInputRouter.RegisterTarget(
            &SceneViewport,
            ViewportClient,
            ViewportClient->GetPlayState() == EViewportPlayState::Playing ? EInteractionDomain::PIE : EInteractionDomain::Editor,
            [this, Index](FRect& OutRect)
            {
                const FViewportRect& ViewportRect = ViewportLayout.GetSceneViewport(Index).GetRect();
                if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
                {
                    return false;
                }

                OutRect = FRect(
                    static_cast<float>(ViewportRect.X),
                    static_cast<float>(ViewportRect.Y),
                    static_cast<float>(ViewportRect.Width),
                    static_cast<float>(ViewportRect.Height));
                return true;
            },
            [this, Index]()
            {
                FEditorViewportClient* Client = ViewportLayout.GetViewportClient(Index);
                return Client ? Client->GetFocusedWorld() : nullptr;
            });
    }
}

void UEditorEngine::WorldTick(float DeltaTime)
{
    // 포커스된 뷰포트의 카메라를 해당 월드의 ActiveCamera로 설정합니다.
    // PIE Possessed 상태에서는 PlayerController의 RuntimeCamera가 게임 카메라를 소유하므로
    // 여기서 Editor viewport camera로 덮어쓰면 WASD/Mouse Look 결과가 화면에 반영되지 않습니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
    if (FocusedClient && FocusedClient->AllowsEditorWorldControl())
    {
        if (UWorld* FocusedWorld = FocusedClient->GetFocusedWorld())
        {
            if (FViewportCamera* Cam = FocusedClient->GetCamera())
            {
                FocusedWorld->SetActiveCamera(Cam);
            }
        }
    }

    // WorldList의 모든 월드에 대해 Tick()을 넣어줍니다.
    // UWorld::Tick에서 EWorldType에 따라 TickEditor / TickGame이 분기됩니다.
    for (FWorldContext& Ctx : WorldList)
    {
        if (!Ctx.World || Ctx.bPaused)
            continue;
        Ctx.World->Tick(DeltaTime);
    }
}

int32 UEditorEngine::DeleteActors(const TArray<AActor*>& Actors)
{
    std::unordered_set<AActor*> SeenActors;
    TArray<std::pair<UWorld*, AActor*>> ActorsToDelete;

    for (AActor* Actor : Actors)
    {
        if (!Actor || SeenActors.find(Actor) != SeenActors.end())
        {
            continue;
        }

        UWorld* ActorWorld = Actor->GetFocusedWorld();
        if (!ActorWorld)
        {
            continue;
        }

        SeenActors.insert(Actor);
        ActorsToDelete.push_back(std::make_pair(ActorWorld, Actor));
    }

    if (ActorsToDelete.empty())
    {
        return 0;
    }

    CaptureUndoSnapshot("Delete Actors");

    int32 DeletedCount = 0;
    SelectionManager.BeginBatchUpdate();
    for (const std::pair<UWorld*, AActor*>& Entry : ActorsToDelete)
    {
        Entry.first->DestroyActor(Entry.second);
        ++DeletedCount;
    }
    SelectionManager.EndBatchUpdate();

    if (DeletedCount > 0)
    {
        MainPanel.GetSceneWidget().MarkSceneDirty();
        MainPanel.PushFooterLog(DeletedCount > 1 ? "Actors deleted" : "Actor deleted");
    }

    return DeletedCount;
}

FString UEditorEngine::CaptureSceneSnapshot() const
{
    UEditorEngine* MutableThis = const_cast<UEditorEngine*>(this);
    FWorldContext* Ctx = MutableThis->GetWorldContextFromHandle(ActiveWorldHandle);
    if (!Ctx || !Ctx->World)
    {
        return "";
    }

    return FSceneSaveManager::SaveToString(*Ctx, nullptr);
}

bool UEditorEngine::CaptureUndoSnapshot(const char* Reason)
{
    if (bRestoringUndoRedo || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Snapshot = CaptureSceneSnapshot();
    if (Snapshot.empty())
    {
        return false;
    }

    if (!UndoHistory.empty() && UndoHistory.back().Snapshot == Snapshot)
    {
        return false;
    }

    FUndoSnapshotEntry Entry;
    Entry.Label = (Reason && Reason[0] != '\0') ? Reason : "Scene Edit";
    Entry.Snapshot = std::move(Snapshot);
    UndoHistory.push_back(std::move(Entry));
    if (static_cast<int32>(UndoHistory.size()) > MaxUndoHistory)
    {
        UndoHistory.erase(UndoHistory.begin());
    }
    if (!RedoHistory.empty())
    {
        RedoHistory.clear();
        MainPanel.PushFooterLog("Redo history cleared");
    }
    return true;
}

bool UEditorEngine::RestoreSceneSnapshot(const FString& Snapshot)
{
    if (Snapshot.empty())
    {
        return false;
    }

    FEditorCameraState CurrentCam;
    if (const FViewportCamera* Cam = GetCamera())
    {
        CurrentCam.Location = Cam->GetLocation();
        CurrentCam.Rotation = FRotator(Cam->GetRotation());
        CurrentCam.FOV = Cam->GetFOV() * (180.f / 3.14159265358979f);
        CurrentCam.NearClip = Cam->GetNearPlane();
        CurrentCam.FarClip = Cam->GetFarPlane();
        CurrentCam.bValid = true;
    }

    FWorldContext LoadCtx;
    FEditorCameraState LoadedCam;
    FSceneSaveManager::LoadFromString(Snapshot, LoadCtx, &LoadedCam);
    if (!LoadCtx.World)
    {
        return false;
    }

    bRestoringUndoRedo = true;
    MainPanel.ResetWidgetSelections();
    ClearScene();

    LoadCtx.WorldType = EWorldType::Editor;
    LoadCtx.ContextHandle = FName("UndoRedoScene");
    LoadCtx.ContextName = "Undo/Redo Scene";
    WorldList.push_back(LoadCtx);
    SetActiveWorld(LoadCtx.ContextHandle);
    ApplySpatialIndexMaintenanceSettings(LoadCtx.World);
    ResetViewport();

    const FEditorCameraState& CameraToRestore = LoadedCam.bValid ? LoadedCam : CurrentCam;
    if (CameraToRestore.bValid)
    {
        if (FViewportCamera* Cam = GetCamera())
        {
            Cam->SetLocation(CameraToRestore.Location);
            Cam->SetRotation(FQuat(CameraToRestore.Rotation));
            Cam->SetFOV(CameraToRestore.FOV * (3.14159265358979f / 180.f));
            Cam->SetNearPlane(CameraToRestore.NearClip);
            Cam->SetFarPlane(CameraToRestore.FarClip);
            if (FEditorViewportClient* Client = ViewportLayout.GetViewportClient(0))
            {
                Client->SyncCameraTarget();
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->RebuildSpatialIndex();
    }
    MainPanel.GetSceneWidget().MarkSceneDirty();
    bRestoringUndoRedo = false;
    return true;
}

bool UEditorEngine::Undo()
{
    if (bRestoringUndoRedo || UndoHistory.empty() || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Current = CaptureSceneSnapshot();
    FUndoSnapshotEntry Previous = std::move(UndoHistory.back());
    UndoHistory.pop_back();
    if (!Current.empty())
    {
        RedoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
        if (static_cast<int32>(RedoHistory.size()) > MaxUndoHistory)
        {
            RedoHistory.erase(RedoHistory.begin());
        }
    }

    const bool bRestored = RestoreSceneSnapshot(Previous.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("Undo: " + Previous.Label);
    }
    return bRestored;
}

bool UEditorEngine::Redo()
{
    if (bRestoringUndoRedo || RedoHistory.empty() || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }

    FString Current = CaptureSceneSnapshot();
    FUndoSnapshotEntry Next = std::move(RedoHistory.back());
    RedoHistory.pop_back();
    if (!Current.empty())
    {
        UndoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
        if (static_cast<int32>(UndoHistory.size()) > MaxUndoHistory)
        {
            UndoHistory.erase(UndoHistory.begin());
        }
    }

    const bool bRestored = RestoreSceneSnapshot(Next.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("Redo: " + Next.Label);
    }
    return bRestored;
}

bool UEditorEngine::RestoreUndoHistoryIndex(int32 Index)
{
    if (bRestoringUndoRedo || GetEditorState() != EViewportPlayState::Editing)
    {
        return false;
    }
    if (Index < 0 || Index >= static_cast<int32>(UndoHistory.size()))
    {
        return false;
    }

    FUndoSnapshotEntry Target = UndoHistory[Index];
    FString Current = CaptureSceneSnapshot();

    RedoHistory.clear();
    if (!Current.empty())
    {
        RedoHistory.push_back(FUndoSnapshotEntry{ "Current", std::move(Current) });
    }
    for (int32 HistoryIndex = static_cast<int32>(UndoHistory.size()) - 1; HistoryIndex > Index; --HistoryIndex)
    {
        RedoHistory.push_back(std::move(UndoHistory[HistoryIndex]));
        if (static_cast<int32>(RedoHistory.size()) > MaxUndoHistory)
        {
            RedoHistory.erase(RedoHistory.begin());
        }
    }

    UndoHistory.erase(UndoHistory.begin() + Index, UndoHistory.end());

    const bool bRestored = RestoreSceneSnapshot(Target.Snapshot);
    if (bRestored)
    {
        MainPanel.PushFooterLog("History restored: " + Target.Label);
    }
    return bRestored;
}

void UEditorEngine::ClearUndoHistory()
{
    const bool bHadHistory = !UndoHistory.empty() || !RedoHistory.empty();
    UndoHistory.clear();
    RedoHistory.clear();
    if (bHadHistory)
    {
        MainPanel.PushFooterLog("Undo history cleared");
    }
}

FUndoHistoryStats UEditorEngine::GetUndoHistoryStats() const
{
    FUndoHistoryStats Stats;
    Stats.UndoCount = static_cast<int32>(UndoHistory.size());
    Stats.RedoCount = static_cast<int32>(RedoHistory.size());
    Stats.MaxEntries = MaxUndoHistory;

    auto Accumulate = [&Stats](const TArray<FUndoSnapshotEntry>& Entries)
    {
        for (const FUndoSnapshotEntry& Entry : Entries)
        {
            Stats.LogicalBytes += Entry.Label.size();
            Stats.LogicalBytes += Entry.Snapshot.size();
            Stats.ReservedBytes += Entry.Label.capacity();
            Stats.ReservedBytes += Entry.Snapshot.capacity();
        }
    };

    Accumulate(UndoHistory);
    Accumulate(RedoHistory);
    Stats.EntryOverheadBytes = (UndoHistory.size() + RedoHistory.size()) * sizeof(FUndoSnapshotEntry);
    Stats.ApproxTotalBytes = Stats.ReservedBytes + Stats.EntryOverheadBytes;
    return Stats;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    MainPanel.Render(DeltaTime);
}

void UEditorEngine::StartPlaySession()
{
    const EViewportPlayState CurrentState = GetEditorState();
    if (CurrentState == EViewportPlayState::Paused)
    {
        ResumePlaySession();
        return;
    }

    if (CurrentState == EViewportPlayState::Playing)
    {
        return;
    }

    bStartPlaySessionQueued = true;
    bStopPlaySessionQueued = false;
}

void UEditorEngine::ProcessQueuedPlaySessionRequests()
{
    if (bStopPlaySessionQueued)
    {
        bStopPlaySessionQueued = false;
        bStartPlaySessionQueued = false;
        StopPlaySessionNow();
        return;
    }

    if (bStartPlaySessionQueued)
    {
        bStartPlaySessionQueued = false;
        StartPlaySessionNow();
    }
}

void UEditorEngine::StartPlaySessionNow()
{
    const EViewportPlayState CurrentState = GetEditorState();

    if (CurrentState == EViewportPlayState::Paused)
    {
        ResumePlaySession();
        return;
    }
    if (CurrentState == EViewportPlayState::Playing) return;

	// 포커스된 뷰포트 클라이언트를 찾고 카메라 상태를 저장한 뒤, 실행 상태를 변경합니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
    UWorld* FocusedWorld = GetFocusedWorld();

    if (!FocusedWorld) return;

    FocusedClient->SaveCameraSnapshot();
    ActivePIEViewportIndex = FocusedIdx;
	// 주의! Editor State는 실제 에디터의 상태가 아닌, 현재 에디터가 포커스한 뷰포트의 상태를 의미합니다.
    SetEditorState(EViewportPlayState::Playing); 

    // PIE 월드 복제하고 세팅한 뒤, RegisterWorld() 헬퍼를 사용해 월드를 WorldList에 등록합니다.
    UWorld* PIEWorld = Cast<UWorld>(FocusedWorld->Duplicate());
    PIEWorld->SetWorldType(EWorldType::PIE);
    FName PIEHandle(("PIE_" + std::to_string(FocusedIdx)).c_str());
    std::string PIEName = "PIE_World_" + std::to_string(FocusedIdx);
    
    RegisterWorld(PIEWorld, EWorldType::PIE, PIEHandle, PIEName);
    ViewportPIEHandles[FocusedIdx] = PIEHandle;

    // 월드를 전환한 뒤 뷰포트에 연결하고, PIE World를 실행합니다.
    SetActiveWorld(PIEHandle);
    FocusedClient->StartPIE(PIEWorld);
    FocusedClient->SetEndPIECallback([this]() { StopPlaySession(); });
    MainPanel.HideEditorWindowsForPIE();

    FocusedClient->LockCursorToViewport();
    InputSystem::Get().SetCursorVisibility(false);
    InputSystem::Get().SetGuiMouseCapture(false);
    InputSystem::Get().SetGuiKeyboardCapture(false);
    InputSystem::Get().SetGuiTextInputCapture(false);
    InputSystem::Get().SetGuiViewportMouseBlock(false);
    InputSystem::Get().SetGuiViewportMouseFocusAllowed(true);
    EditorInputRouter.SetImGuiCaptureState(false, false);
    EditorInputRouter.SetForceViewportMouseBlock(false, true);
    EditorInputRouter.ForceViewportFocus(FocusedClient->GetViewport());
    SelectionManager.ClearSelection();

    constexpr const char* DefaultPIEPlayerControllerClass = "AGameJamPlayerController";
    APlayerController* PlayerController = Cast<APlayerController>(
        PIEWorld->SpawnActorByTypeName(DefaultPIEPlayerControllerClass));
    if (PlayerController)
    {
        PlayerController->InitDefaultComponents();
        PlayerController->SetFName(FName(DefaultPIEPlayerControllerClass));
        PlayerController->ConfigureRuntimeCameraFromViewport(FocusedClient->GetCamera());
        PlayerController->SpawnDefaultPawn();
        FocusedClient->SetPIEPlayerController(PlayerController);
        PIEWorld->SetActiveCamera(PlayerController->GetRuntimeCamera());
    }
    else
    {
        PIEWorld->SetActiveCamera(FocusedClient->GetCamera());
    }
    PIEWorld->BeginPlay();
}

void UEditorEngine::PausePlaySession()
{
    if (GetEditorState() != EViewportPlayState::Playing)
        return;

    SetEditorState(EViewportPlayState::Paused);

    // PIE 컨텍스트를 일시정지 상태로 표시해 WorldTick에서 제외합니다.
    const int32 FocusedIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
        if (FWorldContext* Ctx = GetWorldContextFromHandle(HandleIt->second))
            Ctx->bPaused = true;
}

void UEditorEngine::ResumePlaySession()
{
    const int32 ResumeIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    auto ResumeIt = ViewportPIEHandles.find(ResumeIdx);

    if (ResumeIt != ViewportPIEHandles.end())
    {
        if (FWorldContext* Ctx = GetWorldContextFromHandle(ResumeIt->second))
        {
            Ctx->bPaused = false;
        }
    }

    SetEditorState(EViewportPlayState::Playing);
}

void UEditorEngine::StopPlaySession()
{
    if (GetEditorState() == EViewportPlayState::Editing && ViewportPIEHandles.empty())
    {
        bStopPlaySessionQueued = false;
        return;
    }

    bStopPlaySessionQueued = true;
    bStartPlaySessionQueued = false;
}

void UEditorEngine::StopPlaySessionNow()
{
    if (GetEditorState() == EViewportPlayState::Editing && ViewportPIEHandles.empty())
        return;

    int32 FocusedIdx = (ActivePIEViewportIndex >= 0) ? ActivePIEViewportIndex : ViewportLayout.GetLastFocusedViewportIndex();
    if (ViewportPIEHandles.find(FocusedIdx) == ViewportPIEHandles.end() && !ViewportPIEHandles.empty())
    {
        FocusedIdx = ViewportPIEHandles.begin()->first;
    }
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);

    // 기존 PIE 월드를 해제합니다.
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
    {
        FName PIEHandle = HandleIt->second;
        ViewportPIEHandles.erase(HandleIt);
        
        UnregisterWorld(PIEHandle);
    }

    // 원본 에디터 월드를 검색합니다.
    FName EditorHandle = GetEditorWorldHandle();
    UWorld* EditorWorld = nullptr;
    
    if (EditorHandle != FName::None)
    {
        SetActiveWorld(EditorHandle);
        if (FWorldContext* Ctx = GetWorldContextFromHandle(EditorHandle))
        {
            EditorWorld = Ctx->World;
        }
    }

    // 원본 에디터 월드로 뷰포트 및 상태를 복구합니다.
    ViewportLayout.SetLastFocusedViewportIndex(FocusedIdx);
    FocusedClient->EndPIE(EditorWorld);
    SetEditorState(EViewportPlayState::Editing);
    FocusedClient->RestoreCameraSnapshot();
    ActivePIEViewportIndex = -1;
    MainPanel.RestoreEditorWindowsAfterPIE();

    if (ViewportPIEHandles.empty())
    {
        InputSystem::Get().SetCursorVisibility(true);
    }

    SelectionManager.ClearSelection();
}

void UEditorEngine::ResetViewport()
{
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->CreateCamera();
        ViewportClient->SetWorld(GetWorld());
        ViewportClient->ApplyCameraMode();
    }

    // 디폴트로 0번 뷰포트의 카메라를 월드 활성 카메라로 재등록
    if (UWorld* ActiveWorld = GetWorld())
    {
        ActiveWorld->SetActiveCamera(ViewportLayout.GetIndexedViewportClientCamera(0));
    }
}

void UEditorEngine::SetActiveWorld(const FName& Handle)
{
    UWorld* PreviousWorld = GetWorld();
    if (PreviousWorld)
    {
        UnbindActorDestroyedListener(PreviousWorld);
    }

    UEngine::SetActiveWorld(Handle);

    if (UWorld* ActiveWorld = GetWorld())
    {
        BindActorDestroyedListener(ActiveWorld);
    }
}

void UEditorEngine::CloseScene()
{
    SelectionManager.ClearSelection();
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
        UObjectManager::Get().DestroyObject(Ctx.World);
    }
    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->DestroyCamera();
        ViewportClient->SetWorld(nullptr);
    }
}

void UEditorEngine::NewScene()
{
    ClearScene();
    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    SetActiveWorld(Ctx.ContextHandle);
    ApplySpatialIndexMaintenanceSettings(Ctx.World);
    SpawnDefaultSceneActors(Ctx.World);

    ResetViewport();
}

void UEditorEngine::ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld)
{
    // Init 초반에는 ViewportLayout이 아직 연결되지 않았을 수 있으므로
    // FocusedWorld보다 ActiveWorld(GetWorld) 경로를 우선 사용한다.
    UWorld* World = (TargetWorld != nullptr) ? TargetWorld : GetWorld();
    if (World == nullptr)
    {
        World = GetFocusedWorld();
        if (World == nullptr)
        {
            return;
        }
    }

    const FEditorSettings& Settings = GetSettings();
    FWorldSpatialIndex::FMaintenancePolicy& Policy = World->GetSpatialIndex().GetMaintenancePolicy();

    Policy.BatchRefitMinDirtyCount = std::max<int32>(1, Settings.SpatialBatchRefitMinDirtyCount);
    Policy.BatchRefitDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialBatchRefitDirtyPercentThreshold, 1, 100);
    Policy.RotationStructuralChangeThreshold = std::max<int32>(1, Settings.SpatialRotationStructuralChangeThreshold);
    Policy.RotationDirtyCountThreshold = std::max<int32>(1, Settings.SpatialRotationDirtyCountThreshold);
    Policy.RotationDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialRotationDirtyPercentThreshold, 1, 100);
}

FViewportCamera* UEditorEngine::GetCamera()
{
    return ViewportLayout.GetIndexedViewportClientCamera(0);
}

const FViewportCamera* UEditorEngine::GetCamera() const
{
    return ViewportLayout.GetIndexedViewportClientCamera(0);
}

FEditorRenderPipeline* UEditorEngine::GetEditorRenderPipeline() const
{
    return static_cast<FEditorRenderPipeline*>(GetRenderPipeline());
}

void UEditorEngine::ClearScene()
{
    SelectionManager.ClearSelection();
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::LevelTransition);
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (!ViewportClient)
        {
            continue;
        }
        ViewportClient->DestroyCamera();
        ViewportClient->SetWorld(nullptr);
    }
}

// 이미 생성된 월드를 컨텍스트에 등록합니다.
FWorldContext& UEditorEngine::RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handle, const std::string& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.World = InWorld;
    Context.ContextName = Name;
    Context.ContextHandle = Handle;
    
    WorldList.push_back(Context);
    return WorldList.back();
}

// 컨텍스트에서 월드를 찾아 파괴하고 리스트에서 제거합니다.
void UEditorEngine::UnregisterWorld(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            if (it->World)
            {
                if (it->World == ActorDestroyedListenerWorld)
                {
                    UnbindActorDestroyedListener(it->World);
                }
                it->World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
                UObjectManager::Get().DestroyObject(it->World);
            }
            WorldList.erase(it);
            return; // 찾아서 지웠으므로 즉시 종료
        }
    }
}

// Editor Context World 핸들을 찾아 반환합니다.
FName UEditorEngine::GetEditorWorldHandle() const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::Editor)
        {
            return Ctx.ContextHandle;
        }
    }
    return FName::None;
}

void UEditorEngine::HandleActorDestroyed(AActor* Actor)
{
    SelectionManager.OnActorDestroyed(Actor);
    MainPanel.GetPropertyWidget().OnActorDestroyed(Actor);
    MainPanel.GetMaterialWidget().OnActorDestroyed(Actor);
}

void UEditorEngine::BindActorDestroyedListener(UWorld* World)
{
    if (!World)
    {
        return;
    }

    if (ActorDestroyedListenerWorld == World && ActorDestroyedListenerId != 0)
    {
        return;
    }

    if (ActorDestroyedListenerWorld && ActorDestroyedListenerWorld != World)
    {
        UnbindActorDestroyedListener(ActorDestroyedListenerWorld);
    }

    ActorDestroyedListenerId = World->AddActorDestroyedListener(
        [this](AActor* Actor)
        {
            HandleActorDestroyed(Actor);
        });
    ActorDestroyedListenerWorld = World;
}

void UEditorEngine::UnbindActorDestroyedListener(UWorld* World)
{
    UWorld* TargetWorld = ActorDestroyedListenerWorld ? ActorDestroyedListenerWorld : World;
    if (!TargetWorld || ActorDestroyedListenerId == 0)
    {
        return;
    }

    TargetWorld->RemoveActorDestroyedListener(ActorDestroyedListenerId);
    ActorDestroyedListenerId = 0;
    ActorDestroyedListenerWorld = nullptr;
}
