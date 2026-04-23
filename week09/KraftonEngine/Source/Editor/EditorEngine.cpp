#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"
#include "Viewport/Viewport.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"

IMPLEMENT_CLASS(UEditorEngine, UEngine)

namespace
{
void PreloadDefaultObjAssets(ID3D11Device* Device)
{
    if (!Device)
    {
        return;
    }

    FObjManager::ScanObjSourceFiles();
    const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
    for (const FMeshAssetListItem& Item : ObjFiles)
    {
        if (Item.FullPath.rfind(FPaths::ContentRelativePath("Models/_Basic/"), 0) != 0)
        {
            continue;
        }

        FObjManager::LoadObjStaticMesh(Item.FullPath, Device);
    }
}
} // namespace

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
    // ���� ���� �ʱ�ȭ (Renderer, D3D, �̱��� ��)
    UEngine::Init(InWindow);

    FObjManager::ScanMeshAssets();
    FObjManager::ScanObjSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    PreloadDefaultObjAssets(Renderer.GetFD3DDevice().GetDevice());

    // ������ ���� �ʱ�ȭ
    FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

    MainPanel.Create(Window, Renderer, this);

    // World
    if (WorldList.empty())
    {
        CreateWorldContext(EWorldType::Editor, FName("Default"));
    }
    SetActiveWorld(WorldList[0].ContextHandle);
    GetWorld()->InitWorld();

    // Selection & Gizmo
    SelectionManager.Init();
    SelectionManager.SetWorld(GetWorld());

    // ����Ʈ ���̾ƿ� �ʱ�ȭ + ����� ���� ����
    ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
    ViewportLayout.LoadFromSettings();
}

void UEditorEngine::Shutdown()
{
    // ������ ���� (�������� ����)
    ViewportLayout.SaveToSettings();
    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
    CloseScene();
    SelectionManager.Shutdown();
    MainPanel.Release();
    GPUOcclusion.Release();

    // ����Ʈ ���̾ƿ� ����
    ViewportLayout.Release();

    // ���� ���� ���� (Renderer, D3D ��)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    // ������ �������� �ÿ��� ImGui �г��� ���� ũ�⸦ �����ϹǷ�
    // FViewport RT�� SSplitter ���̾ƿ���� ���� ��������� ó����
}

void UEditorEngine::Tick(float DeltaTime)
{
    // --- PIE ��û ó�� (������ ��迡�� ó���ǵ��� Tick ���ο��� �Һ�) ---
    if (bRequestEndPlayMapQueued)
    {
        bRequestEndPlayMapQueued = false;
        EndPlayMap();
    }
    if (PlaySessionRequest.has_value())
    {
        StartQueuedPlaySessionRequest();
    }

    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        VC->Tick(DeltaTime);
    }

    MainPanel.Update();
    InputSystem::Get().Tick();

    const bool bPIEPaused = IsPausedInEditor();
    const bool bHasPIEWorld = IsPlayingInEditor();
    for (FWorldContext& Ctx : WorldList)
    {
        UWorld* World = Ctx.World;
        if (!World)
        {
            continue;
        }

        if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
        {
            continue;
        }

        ELevelTick TickType = ELevelTick::LEVELTICK_TimeOnly;
        switch (Ctx.WorldType)
        {
        case EWorldType::Editor:
            TickType = ELevelTick::LEVELTICK_ViewportsOnly;
            break;
        case EWorldType::PIE:
        case EWorldType::Game:
            TickType = bPIEPaused ? ELevelTick::LEVELTICK_PauseTick : ELevelTick::LEVELTICK_All;
            break;
        default:
            TickType = ELevelTick::LEVELTICK_TimeOnly;
            break;
        }

        World->Tick(DeltaTime, TickType);
    }

    Render(DeltaTime);
    SelectionManager.Tick();
}

UCameraComponent* UEditorEngine::GetCamera() const
{
    if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
    {
        return ActiveVC->GetCamera();
    }
    return nullptr;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    MainPanel.Render(DeltaTime);
}

// ������ PIE (Play In Editor) ����������������������������������������������������������������
// UE ���� ���: Request�� ���� ����(std::optional)�� ���常 �ϰ� ��� �������� �ʴ´�.
// ���� StartPIE�� ���� Tick ������ StartQueuedPlaySessionRequest���� �Ͼ��.
// ������ UI �ݹ�/Ʈ����� ���� ���� �Ҿ����� Ÿ�̹��� ���ϱ� ����.

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
    // ���� ��û�� UE�� �����ϰ� ����� (��¥ ť �ƴ� ? ���� ����).
    PlaySessionRequest = InParams;
}

void UEditorEngine::CancelRequestPlaySession()
{
    PlaySessionRequest.reset();
}

void UEditorEngine::RequestEndPlayMap()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }
    bRequestEndPlayMapQueued = true;
}

bool UEditorEngine::IsPausedInEditor() const
{
    return PlayInEditorSessionInfo.has_value() && PlayInEditorSessionInfo->bIsPaused;
}

void UEditorEngine::PausePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value() || PlayInEditorSessionInfo->bIsPaused)
    {
        return;
    }

    PlayInEditorSessionInfo->bIsPaused = true;
    if (FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient)
    {
        PIEViewportClient->SetPlayState(EEditorViewportPlayState::Paused);
    }
}

void UEditorEngine::ResumePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value() || !PlayInEditorSessionInfo->bIsPaused)
    {
        return;
    }

    PlayInEditorSessionInfo->bIsPaused = false;
    if (FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient)
    {
        PIEViewportClient->SetPlayState(EEditorViewportPlayState::Playing);
    }
}

void UEditorEngine::TogglePausePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }

    if (PlayInEditorSessionInfo->bIsPaused)
    {
        ResumePlayInEditor();
    }
    else
    {
        PausePlayInEditor();
    }
}

void UEditorEngine::StartQueuedPlaySessionRequest()
{
    if (!PlaySessionRequest.has_value())
    {
        return;
    }

    const FRequestPlaySessionParams Params = *PlaySessionRequest;
    PlaySessionRequest.reset();

    // �̹� PIE ���̸� ���� ������ ���� �� ���� ���� (�ܼ�ȭ).
    if (PlayInEditorSessionInfo.has_value())
    {
        EndPlayMap();
    }

    switch (Params.SessionDestination)
    {
    case EPIESessionDestination::InProcess:
        StartPlayInEditorSession(Params);
        break;
    }
}

void UEditorEngine::StartPlayInEditorSession(const FRequestPlaySessionParams& Params)
{
    // 1) ���� ������ ���带 ������ PIE ���� ���� (UE�� CreatePIEWorldByDuplication ����).
    UWorld* EditorWorld = GetWorld();
    if (!EditorWorld)
    {
        return;
    }
    UWorld* PIEWorld = Cast<UWorld>(EditorWorld->Duplicate(nullptr));
    if (!PIEWorld)
    {
        return;
    }

    // 2) PIE WorldContext�� WorldList�� ���.
    FWorldContext Ctx;
    Ctx.WorldType = EWorldType::PIE;
    Ctx.ContextHandle = FName("PIE");
    Ctx.ContextName = "PIE";
    Ctx.World = PIEWorld;
    if (PIEWorld)
    {
        PIEWorld->SetWorldType(EWorldType::PIE);
    }
    WorldList.push_back(Ctx);

    // 3) ���� ���� ��� (���� Ȱ�� �ڵ� ���� ? EndPlayMap���� ����).
    FLevelEditorViewportClient* PIEViewportClient = Params.DestinationViewportClient ? Params.DestinationViewportClient : ViewportLayout.GetActiveViewport();
    if (!PIEViewportClient)
    {
        return;
    }

    FPlayInEditorSessionInfo Info;
    Info.OriginalRequestParams = Params;
    Info.PIEStartTime = 0.0;
    Info.PreviousActiveWorldHandle = GetActiveWorldHandle();
    Info.DestinationViewportClient = PIEViewportClient;
    if (UCameraComponent* VCCamera = PIEViewportClient->GetCamera())
    {
        Info.SavedViewportCamera.Location = VCCamera->GetWorldLocation();
        Info.SavedViewportCamera.Rotation = VCCamera->GetRelativeRotation();
        Info.SavedViewportCamera.CameraState = VCCamera->GetCameraState();
        Info.SavedViewportCamera.bValid = true;
    }
    PlayInEditorSessionInfo = Info;

    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        if (VC)
        {
            VC->SetPlayState(VC == PIEViewportClient ? EEditorViewportPlayState::Playing : EEditorViewportPlayState::Stopped);
        }
    }

    // 4) ActiveWorldHandle�� PIE�� ��ȯ ? ���� GetWorld()�� PIE ���带 ��ȯ.
    SetActiveWorld(FName("PIE"));

    // GPU Occlusion readback�� ProxyId ����̶� ���尡 ������ stale.
    // ���� ������ ����� ��ȿȭ�ؾ� wrong-proxy hit ����.
    OnRenderSceneCleared();

    // 5) Ȱ�� ����Ʈ ī�޶� PIE ������ ActiveCamera�� ���� ?
    //    LOD ���� ��� ActiveCamera�� �����ϹǷ� ���� �ʿ�.
    if (UCameraComponent* VCCamera = PIEViewportClient->GetCamera())
    {
        PIEWorld->SetActiveCamera(VCCamera);
    }

    // 6) Selection�� PIE ���� �������� ����ε� ? ������ ���͸� ����Ų ä�� �θ�
    //    ��ŷ(=PIE ����) / outliner / outline ������ ��� ��߳���.
    SelectionManager.ClearSelection();
    SelectionManager.SetGizmoEnabled(false); // PIE�� ���۵Ǹ� gizmo ��Ȱ��ȭ
    SelectionManager.SetWorld(PIEWorld);

    // �� �ڵ�� �����Ǵ� �� �Ʒ� EndPlayMap()�� ����.
    // MainPanel.HideEditorWindowsForPIE(); //PIE �߿��� ������ �г��� ����.
    ViewportLayout.DisableWorldAxisForPIE();

    // 7) BeginPlay Ʈ���� ? ��� ���/���ε��� ���� ���� ù Tick ������ ȣ��.
    //    UWorld::BeginPlay�� bHasBegunPlay�� ���� �����ϹǷ� BeginPlay ����
    //    SpawnActor�� ���� �ű� ���͵� �ڵ����� BeginPlay�ȴ�.
    PIEWorld->BeginPlay();
}

void UEditorEngine::EndPlayMap()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }

    // Ȱ�� ���带 PIE ���� �� �ڵ�� ����.
    const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
    SetActiveWorld(PrevHandle);

    // ������ Editor ������ VisibleProxies/ĳ�õ� ī�޶� ���¸� ���� ��ȿȭ.
    // PIE �� Editor WorldTick�� skip�Ǿ� ĳ�ð� PIE ���� �� ���� �״�� ���� �ְ�,
    // NeedsVisibleProxyRebuild()�� ī�޶� ��ȭ ����̶� false�� ��ȯ�ϸ� stale
    // VisibleProxies�� �״�� ����Ǿ� dangling proxy ������ ũ���ð� �� �� �ִ�.
    //
    // ���� Renderer::FrameResources.PerObjectCBPool�� ProxyId�� �ε��̵Ǵ� ���� �� ���� Ǯ�̶�,
    // PIE �� PIE ���Ͻð� ��� ������ �״�� ���� ������ Editor ���Ͻ���
    // bPerObjectCBDirty=false ���·� ���� ���ε尡 skip�Ǿ� PIE ������ transform����
    // �����ȴ�. ��� Editor ���Ͻø� PerObjectCB dirty�� ��ŷ�� ����ε� ����.
    if (UWorld* EditorWorld = GetWorld())
    {
        EditorWorld->GetScene().MarkAllPerObjectCBDirty();

        // ActiveCamera�� PIE ���� �� PIE ����� �Ű����� PIE ����� �Բ� �ı��ƴ�.
        // Editor ������ ActiveCamera�� ������ �� dangling �����͸� ����ų �� �����Ƿ�
        // Ȱ�� ����Ʈ�� ī�޶�� �ٽ� ���ε��� ��� frustum culling�� ���� �����Ѵ�.
        FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient;
        if (PIEViewportClient && PIEViewportClient->GetCamera())
        {
            UCameraComponent* VCCamera = PIEViewportClient->GetCamera();
            if (PlayInEditorSessionInfo->SavedViewportCamera.bValid)
            {
                const FPIEViewportCameraSnapshot& SavedCamera = PlayInEditorSessionInfo->SavedViewportCamera;
                VCCamera->SetWorldLocation(SavedCamera.Location);
                VCCamera->SetRelativeRotation(SavedCamera.Rotation);
                VCCamera->SetCameraState(SavedCamera.CameraState);
            }

            EditorWorld->SetActiveCamera(VCCamera);
        }
        else if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
        {
            if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
            {
                EditorWorld->SetActiveCamera(VCCamera);
            }
        }
    }

    // Selection�� ������ ����� ���� ? PIE ���ʹ� �� �ı��ǹǷ� ���� ����.
    SelectionManager.ClearSelection();
    SelectionManager.SetGizmoEnabled(true); // PIE�� ������ gizmo Ȱ��ȭ
    SelectionManager.SetWorld(GetWorld());

    // �� �ڵ�� �����Ǵ� �� ���� StartPlayInEditorSession()�� ����.
    // MainPanel.RestoreEditorWindowsAfterPIE();
    ViewportLayout.RestoreWorldAxisAfterPIE();

    // PIE WorldContext ���� (DestroyWorldContext�� EndPlay + DestroyObject ����).
    DestroyWorldContext(FName("PIE"));

    // PIE ������ ���Ͻð� ��� �ı������Ƿ� GPU Occlusion readback ��ȿȭ.
    OnRenderSceneCleared();

    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        if (VC)
        {
            VC->SetPlayState(EEditorViewportPlayState::Stopped);
        }
    }

    PlayInEditorSessionInfo.reset();
}

// ������ ���� �޼��� ������������������������������������������������������������������������������������

void UEditorEngine::ResetViewport()
{
    ViewportLayout.ResetViewport(GetWorld());
}

void UEditorEngine::CloseScene()
{
    ClearScene();
}

void UEditorEngine::NewScene()
{
    StopPlayInEditorImmediate();
    ClearScene();
    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    Ctx.World->InitWorld();
    SetActiveWorld(Ctx.ContextHandle);
    SelectionManager.SetWorld(GetWorld());

    ResetViewport();
}

void UEditorEngine::ClearScene()
{
    StopPlayInEditorImmediate();
    SelectionManager.ClearSelection();
    SelectionManager.SetWorld(nullptr);

    // �� ���Ͻ� �ı� �� GPU Occlusion ������¡ ������ ��ȿȭ
    OnRenderSceneCleared();

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay();
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;

    ViewportLayout.DestroyAllCameras();
}


void UEditorEngine::OnRenderSceneCleared()
{
    GPUOcclusion.InvalidateResults();
}

void UEditorEngine::Render(float DeltaTime)
{
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
    FGPUProfiler::Get().BeginFrame();
#endif

    for (FLevelEditorViewportClient* ViewportClient : GetLevelViewportClients())
    {
        SCOPE_STAT_CAT("RenderViewport", "2_Render");
        RenderViewport(ViewportClient);
    }

    Renderer.BeginFrame(SceneView);
    {
        SCOPE_STAT_CAT("EditorUI", "5_UI");
        RenderUI(DeltaTime);
    }

#if STATS
    FGPUProfiler::Get().EndFrame();
#endif

    {
        SCOPE_STAT_CAT("Present", "2_Render");
        Renderer.EndFrame();
    }
}

void UEditorEngine::RenderViewport(FLevelEditorViewportClient* VC)
{
    UCameraComponent* Camera = VC->GetCamera();
    if (!Camera)
        return;

    FViewport* VP = VC->GetViewport();
    if (!VP)
        return;

    ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!Ctx)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    if (!GPUOcclusion.IsInitialized())
        GPUOcclusion.Initialize(Renderer.GetFD3DDevice().GetDevice());

    GPUOcclusion.ReadbackResults(Ctx);

    const FViewportRenderOptions& Opts = VC->GetRenderOptions();
    const FShowFlags& ShowFlags = Opts.ShowFlags;
    EViewMode ViewMode = Opts.ViewMode;

    if (VP->ApplyPendingResize())
    {
        Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
    }

    VP->BeginRender(Ctx);

    RenderTargets.Reset();
    RenderTargets.SetFromViewport(VP);
    FScene& Scene = World->GetScene();
    Scene.ClearFrameData();

    SceneView.SetCameraInfo(Camera);
    SceneView.SetRenderSettings(ViewMode, ShowFlags);
    SceneView.SetRenderOptions(Opts);
    SceneView.SetViewportInfo(VP);
    SceneView.ViewportType = Opts.ViewportType;
    SceneView.OcclusionCulling = &GPUOcclusion;
    SceneView.LODContext = World->PrepareLODContext();

    FViewModeSurfaces* ViewModeSurfaces = nullptr;
    if (const auto* ViewModePassRegistry = Renderer.GetViewModePassRegistry();
        ViewModePassRegistry && ViewModePassRegistry->HasConfig(ViewMode))
    {
        ViewModeSurfaces = Renderer.AcquireViewModeSurfaces(VP, VP->GetWidth(), VP->GetHeight());
    }
    else
    {
        Renderer.ReleaseViewModeSurfaces(VP);
    }

    Renderer.BeginCollect(SceneView, Scene.GetPrimitiveProxyCount());
    auto PipelineContext = Renderer.CreatePipelineContext(SceneView, &RenderTargets, &Scene);
    PipelineContext.ViewMode.ActiveViewMode = ViewMode;
    PipelineContext.ViewMode.Surfaces = ViewModeSurfaces;

    FRenderCollectContext CollectContext = {};
    CollectContext.SceneView = &SceneView;
    CollectContext.Scene = &Scene;
    CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
    CollectContext.ActiveViewMode = ViewMode;
    CollectContext.CollectedPrimitives = const_cast<FCollectedPrimitives*>(&Renderer.GetCollectedPrimitives());

    {
        SCOPE_STAT_CAT("Collector", "3_Collect");

        const bool bIsEditorWorld = (World->GetWorldType() == EWorldType::Editor);

        Renderer.CollectWorld(World, CollectContext);
        Renderer.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Scene);

        if (bIsEditorWorld)
        {
            Renderer.CollectDebugRender(Scene);

            if (ShowFlags.bSceneOctree)
            {
                Renderer.CollectOctreeDebug(World->GetOctree(), Scene);
            }

            if (ShowFlags.bSceneBVH)
            {
                World->BuildWorldPrimitiveVisibleBVHNow();
                Renderer.CollectWorldBVHDebug(World->GetWorldPrimitiveVisibleBVH(), Scene);
            }

            if (ShowFlags.bWorldBound)
            {
                Renderer.CollectWorldBoundsDebug(Renderer.GetCollectedPrimitives().VisibleProxies, Scene);
            }
        }

        // Stat overlays are rendered by ImGui in FLevelViewportLayout.
    }

    {
        SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
        Renderer.BuildDrawCommands(PipelineContext);
        Renderer.RunRootPipeline(ERenderPipelineType::EditorRootPipeline, PipelineContext);
    }

    if (GPUOcclusion.IsInitialized())
    {
        SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");

        GPUOcclusion.DispatchOcclusionTest(
            Ctx,
            VP->GetDepthCopySRV(),
            Renderer.GetCollectedPrimitives().VisibleProxies,
            SceneView.View, SceneView.Proj,
            VP->GetWidth(), VP->GetHeight());
    }
}
