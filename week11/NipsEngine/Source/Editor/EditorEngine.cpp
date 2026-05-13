#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Slate/SlateApplication.h"
#include "Engine/Input/InputSystem.h"
#include "Runtime/ViewportRect.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Stats.h"
#include "Runtime/Script/ScriptManager.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Settings/EditorSettings.h"
#include <algorithm>
#if STATS
#include <chrono>
#endif
#include <filesystem>
#include <unordered_set>
#include <utility>
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

namespace
{
    const char* EditorPlayStateName(EViewportPlayState State)
    {
        switch (State)
        {
        case EViewportPlayState::Editing: return "Editing";
        case EViewportPlayState::Playing: return "Playing";
        case EViewportPlayState::Paused: return "Paused";
        default: return "Unknown";
        }
    }

    const char* WorldTypeName(EWorldType Type)
    {
        switch (Type)
        {
        case EWorldType::Editor: return "Editor";
        case EWorldType::PIE: return "PIE";
        case EWorldType::EditorPriview: return "EditorPreview";
        case EWorldType::ViewerPreview: return "ViewerPreview";
        case EWorldType::Game: return "Game";
        default: return "Unknown";
        }
    }

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

    bool HasCameraComponent(AActor* Actor)
    {
        if (!Actor)
        {
            return false;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (Cast<UCameraComponent>(Component))
            {
                return true;
            }
        }
        return false;
    }

    AActor* FindTaggedPlayerActor(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        for (AActor* Actor : World->GetActors())
        {
            if (Actor && !Actor->IsA<APlayerController>() && !Actor->IsA<APlayerStart>() && Actor->HasTag("Player"))
            {
                return Actor;
            }
        }
        return nullptr;
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
    const std::filesystem::path LogDir = std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs";
    FLog::SetFileOutputPath((LogDir / L"Editor.log").wstring());
    FLog::SetPerfFileOutputPath((LogDir / L"EditorPerf.log").wstring());
    UE_LOG("[EditorEngine] Editor boot started.");

    UEngine::Init(InWindow);
    UndoSystem.SetOwner(this);
    CommandSystem.Initialize(this);
    AssetService.Initialize(this);
    NotificationService.Initialize(this);
    SceneService.Initialize(this);
    GetRmlUiSystem().Initialize(GetRenderer(), "EditorPIE", 1, 1);
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
    const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    ViewportLayout.Init(InWindow, Context->World, Context->SelectionManager, this);
    GetFocusedWorld()->SetActiveCamera(GetCamera());
    ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));

    // Editor용 렌더 파이프라인 세팅
    SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));

    MainPanel.RestoreLastSceneFromProjectSettings();

	FScriptManager::Get().initializeLuaState();
}

void UEditorEngine::Shutdown()
{
    UndoSystem.SetOwner(nullptr);

    // 스플리터 비율을 Settings 에 기록 후 저장
    if (SSplitterV* SV = ViewportLayout.GetRootSplitterV())
        FEditorSettings::Get().SplitterVRatio = SV->GetSplitRatio();
    if (SSplitterH* SH = ViewportLayout.GetTopSplitterH())
        FEditorSettings::Get().SplitterHRatio = SH->GetSplitRatio();

    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());

    CloseScene();
    MainPanel.Release();
    
    // CloseScene 이후에 ViewportLayout을 내리면 Client 포인터 단절로 인한 역참조를 피할 수 있습니다.
    ViewportLayout.Shutdown();           // 위젯 트리 해제 (소유권: UEditorEngine)
    for (auto& ViewerPtr : Viewers)
    {
        ViewerPtr->Shutdown();
    }
    Viewers.clear();
    FSlateApplication::Get().Shutdown(); // RootWindow 해제

    // 엔진 공통 해제 (Renderer, D3D 등)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    ViewportLayout.OnWindowResized(Width, Height);
}

bool UEditorEngine::CanCloseApplication()
{
	return MainPanel.CanCloseEditor();
}


void UEditorEngine::Tick(float DeltaTime)
{
#if STATS
    const auto FrameStart = std::chrono::steady_clock::now();
    const EViewportPlayState StateAtFrameStart = GetEditorState();

    const auto UpdateStart = std::chrono::steady_clock::now();
    const auto UpdateEnd = std::chrono::steady_clock::now();

    const auto PlayRequestStart = std::chrono::steady_clock::now();
#endif
    ProcessQueuedPlaySessionRequests();
#if STATS
    const auto PlayRequestEnd = std::chrono::steady_clock::now();
    const EViewportPlayState StateAfterPlayRequests = GetEditorState();

    const auto InputSetupStart = std::chrono::steady_clock::now();
#endif
    const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
    const bool bGuiKeyboardCaptureForViewport =
        (GuiState.bUsingKeyboard || GuiState.bUsingTextInput) && !GuiState.bAllowViewportMouseFocus;
    EditorInputRouter.SetImGuiCaptureState(GuiState.bUsingMouse, bGuiKeyboardCaptureForViewport);
    EditorInputRouter.SetForceViewportMouseBlock(GuiState.bBlockViewportMouse, GuiState.bAllowViewportMouseFocus);
    RegisterViewportInputTargets();
    if (PIESession.HasPendingViewportInputFocus() && GetEditorState() == EViewportPlayState::Playing)
    {
        const int32 FocusedIdx =
            PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
        if (FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx))
        {
            ViewportLayout.SetLastFocusedViewportIndex(FocusedIdx);
            EditorInputRouter.SetImGuiCaptureState(false, false);
            EditorInputRouter.SetForceViewportMouseBlock(false, true);
            EditorInputRouter.ForceViewportFocus(FocusedClient->GetViewport());
        }
        InputSystem::Get().SetGuiMouseCapture(false);
        InputSystem::Get().SetGuiKeyboardCapture(false);
        InputSystem::Get().SetGuiTextInputCapture(false);
        InputSystem::Get().SetGuiViewportMouseBlock(false);
        InputSystem::Get().SetGuiViewportMouseFocusAllowed(true);
        PIESession.ConsumeViewportInputFocusFrame();
    }
#if STATS
    const auto InputSetupEnd = std::chrono::steady_clock::now();

    const auto InputRouteStart = std::chrono::steady_clock::now();
#endif
    FViewportInputContext RoutedInputContext;
    FInteractionBinding RoutedInputBinding;
    EditorInputRouter.Tick(DeltaTime, RoutedInputContext, RoutedInputBinding);
#if STATS
    const auto InputRouteEnd = std::chrono::steady_clock::now();
#endif

    // Viewer 호버링 상태 동기화 (기존 Layout 외부에 있으므로 수동 동기화 필요)
    for (auto& ViewerPtr : Viewers)
    {
        if (FEditorViewportClient* ViewerClient = static_cast<FEditorViewportClient*>(ViewerPtr->GetViewport().GetClient()))
        {
            if (FEditorViewportState* State = ViewerClient->GetViewportState())
            {
                State->bHovered = (EditorInputRouter.GetFocusedClient() == ViewerClient);
            }
        }
    }

#if STATS
    const auto PanelStart = std::chrono::steady_clock::now();
#endif
    ViewportLayout.Tick(DeltaTime);
    for (auto& ViewerPtr : Viewers)
    {
        ViewerPtr->Tick(DeltaTime);
    }
    MainPanel.Update();
#if STATS
    const auto PanelEnd = std::chrono::steady_clock::now();

    const auto WorldStart = std::chrono::steady_clock::now();
#endif
    WorldTick(DeltaTime);
#if STATS
    const auto WorldEnd = std::chrono::steady_clock::now();

    const auto RenderStart = std::chrono::steady_clock::now();
#endif
    Render(DeltaTime);
#if STATS
    const auto RenderEnd = std::chrono::steady_clock::now();

    static int32 PostPIETraceFrames = 0;
    static int32 PostPIETraceFrameIndex = 0;
    static std::chrono::steady_clock::time_point LastSlowFrameLogTime = {};

    if (StateAtFrameStart != EViewportPlayState::Editing &&
        StateAfterPlayRequests == EViewportPlayState::Editing)
    {
        PostPIETraceFrames = 180;
        PostPIETraceFrameIndex = 0;
        UE_LOG("[EditorFramePerf] PIE stop detected. Tracing next %d editor frames.", PostPIETraceFrames);
    }

    const auto FrameEnd = std::chrono::steady_clock::now();
    auto ToMs = [](std::chrono::steady_clock::duration Duration)
    {
        return std::chrono::duration<double, std::milli>(Duration).count();
    };

    const double FrameMs = ToMs(FrameEnd - FrameStart);
    const bool bPostPIETracing = PostPIETraceFrames > 0;
    const bool bInitialPostPIEFrame = bPostPIETracing && PostPIETraceFrameIndex < 12;
    const bool bPeriodicPostPIEFrame = bPostPIETracing && (PostPIETraceFrameIndex % 30 == 0);
    const auto Now = FrameEnd;
    const bool bCanThrottleLog =
        LastSlowFrameLogTime.time_since_epoch().count() == 0 ||
        std::chrono::duration<double>(Now - LastSlowFrameLogTime).count() >= 0.25;
    const bool bSlowFrame = FrameMs >= 30.0;

    if (bInitialPostPIEFrame || bPeriodicPostPIEFrame || (bSlowFrame && bCanThrottleLog))
    {
        LastSlowFrameLogTime = Now;

        int32 PIEWorldCount = 0;
        int32 PausedWorldCount = 0;
        FString WorldSummary;
        for (const FWorldContext& Ctx : WorldList)
        {
            if (Ctx.WorldType == EWorldType::PIE)
            {
                ++PIEWorldCount;
            }
            if (Ctx.bPaused)
            {
                ++PausedWorldCount;
            }

            if (!WorldSummary.empty())
            {
                WorldSummary += ", ";
            }
            WorldSummary += Ctx.ContextHandle.ToString();
            WorldSummary += ":";
            WorldSummary += WorldTypeName(Ctx.WorldType);
            if (Ctx.ContextHandle == ActiveWorldHandle)
            {
                WorldSummary += "*";
            }
            if (Ctx.bPaused)
            {
                WorldSummary += "(Paused)";
            }
        }
        const int32 VisibleRmlDocuments = GetRmlUiSystem().GetVisibleDocumentCount();

        UE_LOG("[EditorFramePerf] Frame=%.2fms State=%s->%s PostPIEFrame=%d Update=%.2fms PlayReq=%.2fms InputSetup=%.2fms InputRoute=%.2fms Panel=%.2fms World=%.2fms Render=%.2fms Worlds=%zu PIEWorlds=%d PausedWorlds=%d Active=%s PendingScene=%d RmlDocs=%zu VisibleRml=%d Scene=%s | %s",
               FrameMs,
               EditorPlayStateName(StateAtFrameStart),
               EditorPlayStateName(StateAfterPlayRequests),
               bPostPIETracing ? PostPIETraceFrameIndex : -1,
               ToMs(UpdateEnd - UpdateStart),
               ToMs(PlayRequestEnd - PlayRequestStart),
               ToMs(InputSetupEnd - InputSetupStart),
               ToMs(InputRouteEnd - InputRouteStart),
               ToMs(PanelEnd - PanelStart),
               ToMs(WorldEnd - WorldStart),
               ToMs(RenderEnd - RenderStart),
               WorldList.size(),
               PIEWorldCount,
               PausedWorldCount,
               ActiveWorldHandle.ToString().c_str(),
               bPendingSceneOpen ? 1 : 0,
               GetRmlUiSystem().GetDocumentCount(),
               VisibleRmlDocuments,
               CurrentScenePath.c_str(),
               WorldSummary.c_str());
    }

    if (PostPIETraceFrames > 0)
    {
        --PostPIETraceFrames;
        ++PostPIETraceFrameIndex;
    }
#endif
}

void UEditorEngine::RequestPIEViewportInputFocus(int32 FrameCount)
{
    PIESession.RequestViewportInputFocus(FrameCount);
    MainPanel.RequestPIEViewportInputFocus();
}

void UEditorEngine::OnSceneWorldWillUnload(UWorld* OldWorld)
{
    if (OldWorld)
    {
        UnbindActorDestroyedListener(OldWorld);
    }

    GetRmlUiSystem().UnloadAllDocuments();
    GetAudioSystem().StopAll();
    if (OldWorld)
    {
        OldWorld->SetGlobalTimeScale(1.0f);
    }
}

void UEditorEngine::OnSceneWorldLoaded(UWorld* NewWorld)
{
    if (!NewWorld)
    {
        return;
    }

    ApplySpatialIndexMaintenanceSettings(NewWorld);

    if (NewWorld->GetWorldType() == EWorldType::PIE)
    {
        const int32 FocusedIdx =
            PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
        FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
        if (!FocusedClient)
        {
            return;
        }

        FocusedClient->StartPIE(NewWorld);

        SpawnPIEPlayerController(NewWorld, FocusedClient);

        InputSystem::Get().SetUseRawMouse(false);
        RequestPIEViewportInputFocus(3);
        return;
    }

    ResetViewport();
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
            [this, Index]() -> UWorld*
            {
                FEditorViewportClient* Client = ViewportLayout.GetViewportClient(Index);
                return Client ? Client->GetFocusedWorld() : nullptr;
            },
            [this, Index]() -> int32
            {
                // EditorViewport를 감싸는 ImGui window 이름
                ImGuiWindow* Win = ImGui::FindWindowByName("Viewport");
                if (!Win)
                    return 0;

                ImGuiContext* Ctx = ImGui::GetCurrentContext();
                if (!Ctx)
                    return 0;

                for (int32 i = 0; i < Ctx->Windows.Size; ++i)
                {
                    if (Ctx->Windows[i] == Win)
                        return i;
                }
                return 0;
            });
    }

    // Viewer 등록
    for (int32 Index = 0; Index < (int32)Viewers.size(); ++Index)
    {
        auto& ViewerPtr = Viewers[Index];
        FSceneViewport& ViewerViewport = ViewerPtr->GetViewport();
        FEditorViewportClient* ViewerClient = static_cast<FEditorViewportClient*>(ViewerViewport.GetClient());
        if (ViewerClient)
        {
            EditorInputRouter.RegisterTarget(
                &ViewerViewport,
                ViewerClient,
                EInteractionDomain::Editor,
                [this, Index](FRect& OutRect)
                {
                    if (Index >= (int32)Viewers.size())
                        return false;

                    // Viewer ImGui window가 실제로 보이는지 확인
                    char WindowName[64];
                    FEditorViewer* ViewerPtr = Viewers[Index].get();
                    sprintf_s(WindowName, "Viewer##%p", ViewerPtr);
                    ImGuiWindow* Win = ImGui::FindWindowByName(WindowName);
                    if (!Win || !Win->WasActive || Win->Hidden)
                    {
                        return false;
                    }

                    const FViewportRect& ViewportRect = Viewers[Index]->GetViewport().GetRect();
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
                [this, Index]() -> UWorld*
                {
                    FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(Viewers[Index]->GetViewport().GetClient());
                    return Client ? Client->GetFocusedWorld() : nullptr;
                },
                [this, Index]() -> int32
                {
                    char WindowName[64];
                    FEditorViewer* ViewerRawPtr = Viewers[Index].get();
                    sprintf_s(WindowName, "Viewer##%p", ViewerRawPtr);

                    ImGuiContext* Ctx = ImGui::GetCurrentContext();
                    if (!Ctx)
                        return 0;

                    for (int32 i = 0; i < Ctx->Windows.Size; ++i)
                    {
                        if (Ctx->Windows[i] && strcmp(Ctx->Windows[i]->Name, WindowName) == 0)
                        {
                            return i; // 배열 뒤쪽일수록 앞에 그려짐
                        }
                    }
                    return 0;
                });
        }
    }
}

void UEditorEngine::WorldTick(float DeltaTime)
{
    // 포커스된 뷰포트의 카메라를 해당 월드의 ActiveCamera로 설정합니다.
    // PIE Possessed 상태에서는 PlayerController의 RuntimeCamera가 게임 카메라를 소유하므로
    // 여기서 Editor viewport camera로 덮어쓰면 WASD/Mouse Look 결과가 화면에 반영되지 않습니다.
    FEditorViewportClient* FocusedClient =
        static_cast<FEditorViewportClient*>(EditorInputRouter.GetFocusedClient());

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

    ProcessPendingSceneOpen();
}

FEditorViewer* UEditorEngine::CreateViewer(FString InFileName)
{
    for (const auto& Viewer : Viewers)
    {
        if (Viewer->GetFileName() == InFileName)
        {
            MainPanel.OpenViewer(Viewer.get());
            return Viewer.get();
        }
    }

    static int32 ViewerCounter = 0;
    FString HandleStr = "__ViewerPreview_" + std::to_string(ViewerCounter++);
    FName Handle(HandleStr.c_str());

    FWorldContext& ViewerCtx = CreateWorldContext(EWorldType::ViewerPreview, Handle, "Viewer Preview");
    ApplySpatialIndexMaintenanceSettings(ViewerCtx.World);
    SpawnDefaultSceneActors(ViewerCtx.World);

    auto NewViewer = std::make_unique<FEditorViewer>();
    NewViewer->Init(Window, this, ViewerCtx.World, ViewerCtx.SelectionManager);
    NewViewer->ChangeTarget(InFileName);
    MainPanel.OpenViewer(NewViewer.get());

    FEditorViewer* Result = NewViewer.get();
    Viewers.push_back(std::move(NewViewer));

    return Result;
}

void UEditorEngine::RemoveViewer(FEditorViewer* InViewer)
{
    for (auto it = Viewers.begin(); it != Viewers.end(); ++it)
    {
        if (it->get() == InViewer)
        {
            MainPanel.CloseViewer(InViewer);
			
            // Find world handle and unregister
            if (FEditorViewportClient* Client = static_cast<FEditorViewportClient*>((*it)->GetViewport().GetClient()))
            {
                if (UWorld* World = Client->GetFocusedWorld())
                {
                    if (FWorldContext* Ctx = GetWorldContextFromWorld(World))
                    {
                        UnregisterWorld(Ctx->ContextHandle);
                    }
                }
            }

            (*it)->Shutdown();
            Viewers.erase(it);
            return;
        }
    }
}

int32 UEditorEngine::DeleteActors(const TArray<AActor*>& Actors)
{
    if (Actors.empty())
        return 0;

    // 1. World별로 그룹핑
    std::unordered_map<UWorld*, TArray<AActor*>> ActorsByWorld;
    std::unordered_set<AActor*> Seen;

    for (AActor* Actor : Actors)
    {
        if (!Actor || Seen.contains(Actor))
            continue;

        UWorld* World = Actor->GetFocusedWorld();
        if (!World)
            continue;

        Seen.insert(Actor);
        ActorsByWorld[World].push_back(Actor);
    }

    if (ActorsByWorld.empty())
        return 0;

    // 2. Undo는 전체 snapshot
    UndoSystem.CaptureSnapshot("Delete Actors");

    int32 DeletedCount = 0;

    // 3. WorldContext 단위 처리
    for (auto& [World, ActorList] : ActorsByWorld)
    {
        FWorldContext* Context = GetWorldContextFromWorld(World);
        if (!Context)
            continue;

        Context->SelectionManager->BeginBatchUpdate();

        for (AActor* Actor : ActorList)
        {
            if (!Actor)
                continue;

            // Selection 정리 (중요)
            Context->SelectionManager->Deselect(Actor);
            World->DestroyActor(Actor);
            ++DeletedCount;
        }

        Context->SelectionManager->EndBatchUpdate();
    }

    // 4. UI 업데이트
    if (DeletedCount > 0)
    {
        SceneService.MarkDirty();
        NotificationService.Info(DeletedCount > 1 ? "Actors deleted" : "Actor deleted");
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

bool UEditorEngine::RestoreSceneSnapshot(const FString& Snapshot, const FName& RestoreWorldHandle)
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

    UndoSystem.BeginRestore();
    MainPanel.ResetWidgetSelections();
    ClearScene();

    LoadCtx.WorldType = EWorldType::Editor;
    LoadCtx.ContextHandle = RestoreWorldHandle != FName::None ? RestoreWorldHandle : FName("UndoRedoScene");
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
    SceneService.MarkDirty();
    UndoSystem.EndRestore();
    return true;
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

APlayerController* UEditorEngine::SpawnPIEPlayerController(UWorld* PIEWorld, FEditorViewportClient* FocusedClient)
{
    if (!PIEWorld || !FocusedClient)
    {
        return nullptr;
    }

    constexpr const char* DefaultPIEPlayerControllerClass = "APlayerController";

    AGameModeBase* GameMode = PIEWorld->SpawnActor<AGameModeBase>();
    if (!GameMode)
    {
        PIEWorld->SetActiveCamera(FocusedClient->GetCamera());
        UE_LOG_ERROR("[EditorEngine] Failed to spawn PIE GameMode.");
        return nullptr;
    }

    GameMode->SetFName(FName("PIE_GameMode"));
    GameMode->SetPlayerControllerClass(DefaultPIEPlayerControllerClass);

    APlayerController* PlayerController = GameMode->EnsurePlayerController(
        FocusedClient->GetCamera(),
        0,
        0,
        FocusedClient->GetCamera());
    if (PlayerController)
    {
        FocusedClient->SetPIEPlayerController(PlayerController);
    }

    return PlayerController;
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
    FWorldContext* EditorContext = nullptr;
    const FName EditorHandle = GetEditorWorldHandle();
    if (EditorHandle != FName::None)
    {
        EditorContext = GetWorldContextFromHandle(EditorHandle);
    }
    UWorld* SourceWorld = EditorContext ? EditorContext->World : GetFocusedWorld();

    if (!FocusedClient || !SourceWorld) return;
    if (!HasPlayerStart(SourceWorld))
    {
        UE_LOG_WARNING("[PIE] Player Start is missing. PlayerController will use the viewport camera as fallback.");
    }
    AActor* PlayerActor = FindTaggedPlayerActor(SourceWorld);
    if (!PlayerActor)
    {
        UE_LOG_WARNING("[PIE] Player actor with tag 'Player' is missing. Fallback camera actor will be created.");
    }
    else if (!HasCameraComponent(PlayerActor))
    {
        UE_LOG_WARNING("[PIE] Player actor has no CameraComponent. Fallback camera actor will be created instead: %s",
            PlayerActor->GetFName().ToString().c_str());
    }

    bPendingSceneOpen = false;
    PendingSceneOpenPath.clear();
    CurrentScenePath = SceneService.GetCurrentScenePath();
    SourceWorld->SetGlobalTimeScale(1.0f);
    SetRuntimeInputMode(ERuntimeInputMode::GameAndUI);
    GetRmlUiSystem().UnloadGameplayDocuments();
    FScriptManager::Get().ResetLuaState();

    FocusedClient->SaveCameraSnapshot();
    PIESession.SetActiveViewportIndex(FocusedIdx);
	// 주의! Editor State는 실제 에디터의 상태가 아닌, 현재 에디터가 포커스한 뷰포트의 상태를 의미합니다.
    SetEditorState(EViewportPlayState::Playing); 

    // PIE 월드 복제하고 세팅한 뒤, RegisterWorld() 헬퍼를 사용해 월드를 WorldList에 등록합니다.
    UWorld* PIEWorld = Cast<UWorld>(SourceWorld->Duplicate());
    PIEWorld->SetWorldType(EWorldType::PIE);
    FName PIEHandle(("PIE_" + std::to_string(FocusedIdx)).c_str());
    std::string PIEName = "PIE_World_" + std::to_string(FocusedIdx);
    
    RegisterWorld(PIEWorld, EWorldType::PIE, PIEHandle, PIEName);
    PIESession.RegisterViewportWorld(FocusedIdx, PIEHandle);

    // 월드를 전환한 뒤 뷰포트에 연결하고, PIE World를 실행합니다.
    SetActiveWorld(PIEHandle);
    FocusedClient->StartPIE(PIEWorld);
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
    RequestPIEViewportInputFocus(3);

	for (auto& Ctx : WorldList)
	{
        Ctx.SelectionManager->ClearSelection();
	}

    SpawnPIEPlayerController(PIEWorld, FocusedClient);
    PIEWorld->BeginPlay();
}

void UEditorEngine::PausePlaySession()
{
    if (GetEditorState() != EViewportPlayState::Playing)
        return;

    SetEditorState(EViewportPlayState::Paused);

    // PIE 컨텍스트를 일시정지 상태로 표시해 WorldTick에서 제외합니다.
    const int32 FocusedIdx = PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
    FName PIEHandle;
    if (PIESession.FindViewportWorldHandle(FocusedIdx, PIEHandle))
        if (FWorldContext* Ctx = GetWorldContextFromHandle(PIEHandle))
            Ctx->bPaused = true;
}

void UEditorEngine::ResumePlaySession()
{
    const int32 ResumeIdx = PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
    FName PIEHandle;

    if (PIESession.FindViewportWorldHandle(ResumeIdx, PIEHandle))
    {
        if (FWorldContext* Ctx = GetWorldContextFromHandle(PIEHandle))
        {
            Ctx->bPaused = false;
        }
    }

    SetEditorState(EViewportPlayState::Playing);
}

void UEditorEngine::StopPlaySession()
{
    if (GetEditorState() == EViewportPlayState::Editing && !PIESession.HasAnyViewportWorld())
    {
        bStopPlaySessionQueued = false;
        return;
    }

    bStopPlaySessionQueued = true;
    bStartPlaySessionQueued = false;
}

void UEditorEngine::StopPlaySessionNow()
{
#if STATS
    const auto StopStart = std::chrono::steady_clock::now();
#endif
    if (GetEditorState() == EViewportPlayState::Editing && !PIESession.HasAnyViewportWorld())
        return;

#if STATS
    const auto PrepStart = std::chrono::steady_clock::now();
#endif
    bPendingSceneOpen = false;
    PendingSceneOpenPath.clear();
    CurrentScenePath.clear();
    if (UWorld* World = GetWorld())
    {
        World->SetGlobalTimeScale(1.0f);
    }
    SetRuntimeInputMode(ERuntimeInputMode::GameAndUI);

    int32 FocusedIdx = PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
    FocusedIdx = PIESession.ResolveRegisteredViewportIndex(FocusedIdx);
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
#if STATS
    const auto PrepEnd = std::chrono::steady_clock::now();

    // 기존 PIE 월드를 해제합니다.
    const auto WorldCleanupStart = std::chrono::steady_clock::now();
#endif
    FName PIEHandle;
    if (PIESession.RemoveViewportWorld(FocusedIdx, PIEHandle))
    {
        UnregisterWorld(PIEHandle);

        // PIE에서 Lua Audio API로 직접 재생한 전역 사운드는 Actor EndPlay 소유가 아니므로
        // 마지막 PIE 세션이 끝날 때 명시적으로 정리합니다.
        if (!PIESession.HasAnyViewportWorld())
        {
            GetAudioSystem().StopAll();
        }
    }
#if STATS
    const auto WorldCleanupEnd = std::chrono::steady_clock::now();

    // 원본 에디터 월드를 검색합니다.
    const auto RestoreWorldStart = std::chrono::steady_clock::now();
#endif
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
#if STATS
    const auto RestoreWorldEnd = std::chrono::steady_clock::now();

    // 원본 에디터 월드로 뷰포트 및 상태를 복구합니다.
    const auto ViewportRestoreStart = std::chrono::steady_clock::now();
#endif
    ViewportLayout.SetLastFocusedViewportIndex(FocusedIdx);
    FocusedClient->EndPIE(EditorWorld);
    SetEditorState(EViewportPlayState::Editing);
    FocusedClient->RestoreCameraSnapshot();
    PIESession.ClearActiveViewportIndex();
    MainPanel.RestoreEditorWindowsAfterPIE();

    if (!PIESession.HasAnyViewportWorld())
    {
        InputSystem::Get().SetCursorVisibility(true);
    }

	for (auto& Ctx : WorldList)
	{
        Ctx.SelectionManager->ClearSelection();
	}

#if STATS
    const auto ViewportRestoreEnd = std::chrono::steady_clock::now();

    const auto RmlUnloadStart = std::chrono::steady_clock::now();
#endif
    GetRmlUiSystem().UnloadGameplayDocuments();
#if STATS
    const auto RmlUnloadEnd = std::chrono::steady_clock::now();

    const auto LuaResetStart = std::chrono::steady_clock::now();
#endif
    FScriptManager::Get().ResetLuaState();
#if STATS
    const auto LuaResetEnd = std::chrono::steady_clock::now();

    const auto StopEnd = std::chrono::steady_clock::now();
    auto ToMs = [](std::chrono::steady_clock::duration Duration)
    {
        return std::chrono::duration<double, std::milli>(Duration).count();
    };
    UE_LOG("[PIEPerf] Stop Total=%.2fms Prep=%.2fms WorldCleanup=%.2fms RestoreWorld=%.2fms ViewportRestore=%.2fms RmlUnload=%.2fms LuaReset=%.2fms RemainingWorlds=%zu Active=%s RmlDocs=%zu",
           ToMs(StopEnd - StopStart),
           ToMs(PrepEnd - PrepStart),
           ToMs(WorldCleanupEnd - WorldCleanupStart),
           ToMs(RestoreWorldEnd - RestoreWorldStart),
           ToMs(ViewportRestoreEnd - ViewportRestoreStart),
           ToMs(RmlUnloadEnd - RmlUnloadStart),
           ToMs(LuaResetEnd - LuaResetStart),
           WorldList.size(),
           ActiveWorldHandle.ToString().c_str(),
           GetRmlUiSystem().GetDocumentCount());
#endif
}

void UEditorEngine::ResetViewport()
{
    FWorldContext* ActiveContext = GetWorldContextFromHandle(ActiveWorldHandle);
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (ViewportClient)
        {
            ViewportClient->CreateCamera();
            ViewportClient->SetWorld(GetWorld());
            ViewportClient->SetSelectionManager(ActiveContext ? ActiveContext->SelectionManager : nullptr);
            ViewportClient->SetGizmo(ActiveContext && ActiveContext->SelectionManager ? ActiveContext->SelectionManager->GetGizmo() : nullptr);
            ViewportClient->ApplyCameraMode();
        }
    }

    for (auto& ViewerPtr : Viewers)
    {
        FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(ViewerPtr->GetViewport().GetClient());
        if (ViewportClient)
        {
            ViewportClient->CreateCamera();
            ViewportClient->SetWorld(ViewportClient->GetFocusedWorld());
            ViewportClient->ApplyCameraMode();
        }
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
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
        Ctx.SelectionManager->ClearSelection();
        Ctx.SelectionManager->Shutdown();
        delete Ctx.SelectionManager;
        Ctx.SelectionManager = nullptr;
        UObjectManager::Get().DestroyObject(Ctx.World);
    }
    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (ViewportClient)
        {
            ViewportClient->DestroyCamera();
            ViewportClient->SetWorld(nullptr);
        }
    }

    for (auto& ViewerPtr : Viewers)
    {
        MainPanel.CloseViewer(ViewerPtr.get());
        if (FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(ViewerPtr->GetViewport().GetClient()))
        {
            ViewportClient->DestroyCamera();
            ViewportClient->SetWorld(nullptr);
        }
    }

	Viewers.clear();
}

void UEditorEngine::NewScene()
{
    ClearScene();
    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    SetActiveWorld(Ctx.ContextHandle);
    ApplySpatialIndexMaintenanceSettings(Ctx.World);
    SpawnDefaultSceneActors(Ctx.World);

	// Selection & Gizmo
    const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    ViewportLayout.Init(Window, Context->World, Context->SelectionManager, this);
    ResetViewport();
    GetFocusedWorld()->SetActiveCamera(GetCamera());

    // Slate 초기화 및 Viewport Layout 추가
    FSlateApplication::Get().Initialize();
    ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));
}

bool UEditorEngine::CreateDefaultSceneAsset(const FString& FilePath)
{
    if (FilePath.empty())
    {
        return false;
    }

    static int32 DefaultSceneAssetCounter = 0;
    ++DefaultSceneAssetCounter;
    const FString HandleName = "__DefaultSceneAsset_" + std::to_string(DefaultSceneAssetCounter);
    const FName Handle(HandleName.c_str());

    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, Handle, "Default Scene Asset");
    ApplySpatialIndexMaintenanceSettings(Ctx.World);
    SpawnDefaultSceneActors(Ctx.World);

    const bool bSaved = FSceneSaveManager::SaveToFilePath(FilePath, Ctx, nullptr);
    UnregisterWorld(Handle);
    return bSaved;
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

UWorld* UEditorEngine::GetFocusedWorld() const
{
	const FEditorViewportClient* FocusedClient = static_cast<FEditorViewportClient*>(EditorInputRouter.GetFocusedClient());

	if (!FocusedClient)
	{
        return ViewportLayout.GetViewportClient(ViewportLayout.GetLastFocusedViewportIndex())->GetFocusedWorld();
	}

    return FocusedClient ? FocusedClient->GetFocusedWorld() : nullptr;
}

FWorldContext* UEditorEngine::GetFocusedWorldContext()
{
	if (UWorld* World = GetFocusedWorld())
	{
        return GetWorldContextFromWorld(World);
	}

    return nullptr;
}

EViewportPlayState UEditorEngine::GetEditorState() const
{
    const int32 StateViewportIndex =
        PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());

	const FEditorViewportClient* FocusedClient = static_cast<FEditorViewportClient*>(EditorInputRouter.GetFocusedClient());

    return FocusedClient ? FocusedClient->GetPlayState() : EViewportPlayState::Editing;
}

void UEditorEngine::SetEditorState(EViewportPlayState InState)
{
    const int32 StateViewportIndex =
        PIESession.ResolveActiveViewportIndex(ViewportLayout.GetLastFocusedViewportIndex());
    FEditorViewportClient* FocusedClient = ViewportLayout.GetViewportClient(StateViewportIndex);
    if (!FocusedClient)
    {
        return;
    }

    FocusedClient->SetPlayState(InState);
}

FEditorRenderPipeline* UEditorEngine::GetEditorRenderPipeline() const
{
    return static_cast<FEditorRenderPipeline*>(GetRenderPipeline());
}

void UEditorEngine::ClearScene()
{
    UnbindActorDestroyedListener(ActorDestroyedListenerWorld);
 
    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay(EEndPlayReason::Type::LevelTransition);
        Ctx.SelectionManager->ClearSelection();
        Ctx.SelectionManager->Shutdown();
        delete Ctx.SelectionManager;
        Ctx.SelectionManager = nullptr;
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;

    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        FEditorViewportClient* ViewportClient = ViewportLayout.GetViewportClient(i);
        if (ViewportClient)
        {
            ViewportClient->DestroyCamera();
            ViewportClient->SetWorld(nullptr);
        }
    }
	
    for (auto& ViewerPtr : Viewers)
    {
        MainPanel.CloseViewer(ViewerPtr.get());
        ViewerPtr->ClearViewTarget();

        if (FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(ViewerPtr->GetViewport().GetClient()))
        {
            ViewportClient->DestroyCamera();
            ViewportClient->SetWorld(nullptr);
        }
    }

	Viewers.clear();
}

// 이미 생성된 월드를 컨텍스트에 등록합니다.
FWorldContext& UEditorEngine::RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handle, const std::string& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.World = InWorld;
    Context.ContextName = Name;
    Context.ContextHandle = Handle;
    Context.SelectionManager = new FSelectionManager;
    Context.SelectionManager->Init();
    
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
            if (!UndoSystem.IsRestoring())
            {
                UndoSystem.ClearHistory(Handle);
            }
            if (it->World)
            {
                if (it->World == ActorDestroyedListenerWorld)
                {
                    UnbindActorDestroyedListener(it->World);
                }
                it->World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);				
                UObjectManager::Get().DestroyObject(it->World);
            }

			if (it->SelectionManager)
			{
                it->SelectionManager->Shutdown();
                delete it->SelectionManager;
                it->SelectionManager = nullptr;
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
	UWorld* World = Actor->GetFocusedWorld();
    const FWorldContext* Ctx = GetWorldContextFromWorld(World);
    Ctx->SelectionManager->OnActorDestroyed(Actor);

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
