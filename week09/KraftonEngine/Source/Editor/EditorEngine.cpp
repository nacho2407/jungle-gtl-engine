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
#include "Viewport/Viewport.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Render/Core/PassTypes.h"

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
		if (Item.FullPath.rfind("Data/BasicShape/", 0) != 0)
		{
			continue;
		}

		FObjManager::LoadObjStaticMesh(Item.FullPath, Device);
	}
}
}

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);

	FObjManager::ScanMeshAssets();
	FObjManager::ScanObjSourceFiles();
	FMaterialManager::Get().ScanMaterialAssets();
	PreloadDefaultObjAssets(Renderer.GetFD3DDevice().GetDevice());

	// 에디터 전용 초기화
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

	// 뷰포트 레이아웃 초기화 + 저장된 설정 복원
	ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
	ViewportLayout.LoadFromSettings();

}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	ViewportLayout.SaveToSettings();
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// 뷰포트 레이아웃 해제
	ViewportLayout.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	// 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
	// FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
	// --- PIE 요청 처리 (프레임 경계에서 처리되도록 Tick 선두에서 소비) ---
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

// ─── PIE (Play In Editor) ────────────────────────────────
// UE 패턴 요약: Request는 단일 슬롯(std::optional)에 저장만 하고 즉시 실행하지 않는다.
// 실제 StartPIE는 다음 Tick 선두의 StartQueuedPlaySessionRequest에서 일어난다.
// 이유는 UI 콜백/트랜잭션 도중 같은 불안정한 타이밍을 피하기 위함.

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
	// 동시 요청은 UE와 동일하게 덮어쓴다 (진짜 큐 아님 — 단일 슬롯).
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

	// 이미 PIE 중이면 기존 세션을 정리 후 새로 시작 (단순화).
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
	// 1) 현재 에디터 월드를 복제해 PIE 월드 생성 (UE의 CreatePIEWorldByDuplication 대응).
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

	// 2) PIE WorldContext를 WorldList에 등록.
	FWorldContext Ctx;
	Ctx.WorldType = EWorldType::PIE;
	Ctx.ContextHandle = FName("PIE");
	Ctx.ContextName = "PIE";
	Ctx.World = PIEWorld;
	WorldList.push_back(Ctx);

	// 3) 세션 정보 기록 (이전 활성 핸들 포함 — EndPlayMap에서 복원).
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

	// 4) ActiveWorldHandle을 PIE로 전환 — 이후 GetWorld()는 PIE 월드를 반환.
	SetActiveWorld(FName("PIE"));

	// GPU Occlusion readback은 ProxyId 기반이라 월드가 갈리면 stale.
	// 이전 프레임 결과를 무효화해야 wrong-proxy hit 방지.
	OnRenderSceneCleared();

	// 5) 활성 뷰포트 카메라를 PIE 월드의 ActiveCamera로 설정 —
	//    LOD 갱신 등에서 ActiveCamera를 참조하므로 설정 필요.
	if (UCameraComponent* VCCamera = PIEViewportClient->GetCamera())
	{
		PIEWorld->SetActiveCamera(VCCamera);
	}

	// 6) Selection을 PIE 월드 기준으로 재바인딩 — 에디터 액터를 가리킨 채로 두면
	//    픽킹(=PIE 월드) / outliner / outline 렌더가 모두 어긋난다.
	SelectionManager.ClearSelection();
	//SelectionManager.SetGizmoEnabled(false); //PIE가 시작되면 gizmo 비활성화
	SelectionManager.SetWorld(PIEWorld);
	
	//이 코드와 대응되는 게 아래 EndPlayMap()에 있음.
	//MainPanel.HideEditorWindowsForPIE(); //PIE 중에는 에디터 패널을 숨김.
	//ViewportLayout.DisableWorldAxisForPIE(); //PIE 중에는 월드 축 렌더링을 비활성화.

	// 7) BeginPlay 트리거 — 모든 등록/바인딩이 끝난 다음 첫 Tick 이전에 호출.
	//    UWorld::BeginPlay가 bHasBegunPlay를 먼저 세팅하므로 BeginPlay 도중
	//    SpawnActor로 만든 신규 액터도 자동으로 BeginPlay된다.
	PIEWorld->BeginPlay();
}

void UEditorEngine::EndPlayMap()
{
	if (!PlayInEditorSessionInfo.has_value())
	{
		return;
	}

	// 활성 월드를 PIE 시작 전 핸들로 복원.
	const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
	SetActiveWorld(PrevHandle);

	// 복귀한 Editor 월드의 VisibleProxies/캐시된 카메라 상태를 강제 무효화.
	// PIE 중 Editor WorldTick이 skip되어 캐시가 PIE 시작 전 시점 그대로 남아 있고,
	// NeedsVisibleProxyRebuild()가 카메라 변화 기반이라 false를 반환하면 stale
	// VisibleProxies가 그대로 재사용되어 dangling proxy 참조로 크래시가 날 수 있다.
	//
	// 또한 Renderer::PerObjectCBPool은 ProxyId로 인덱싱되는 월드 간 공유 풀이라,
	// PIE 중 PIE 프록시가 덮어쓴 슬롯이 그대로 남아 있으면 Editor 프록시의
	// bPerObjectCBDirty=false 상태로 인해 업로드가 skip되어 PIE 마지막 transform으로
	// 렌더된다. 모든 Editor 프록시를 PerObjectCB dirty로 마킹해 재업로드 강제.
	if (UWorld* EditorWorld = GetWorld())
	{
		EditorWorld->GetScene().MarkAllPerObjectCBDirty();

		// ActiveCamera는 PIE 시작 시 PIE 월드로 옮겨졌고 PIE 월드와 함께 파괴됐다.
		// Editor 월드의 ActiveCamera는 여전히 그 dangling 포인터를 가리킬 수 있으므로
		// 활성 뷰포트의 카메라로 다시 바인딩해 줘야 frustum culling이 정상 동작한다.
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

	// Selection을 에디터 월드로 복원 — PIE 액터는 곧 파괴되므로 먼저 비운다.
	SelectionManager.ClearSelection();
	//SelectionManager.SetGizmoEnabled(true); //PIE가 끝나면 gizmo 활성화
	SelectionManager.SetWorld(GetWorld());
	
	//이 코드와 대응되는 게 위의 StartPlayInEditorSession()에 있음.
	//MainPanel.RestoreEditorWindowsAfterPIE();
	//ViewportLayout.RestoreWorldAxisAfterPIE();

	// PIE WorldContext 제거 (DestroyWorldContext가 EndPlay + DestroyObject 수행).
	DestroyWorldContext(FName("PIE"));

	// PIE 월드의 프록시가 모두 파괴됐으므로 GPU Occlusion readback 무효화.
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

// ─── 기존 메서드 ──────────────────────────────────────────

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

	// 씬 프록시 파괴 전 GPU Occlusion 스테이징 데이터 무효화
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

	Renderer.BeginFrame();
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
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	UWorld* World = GetWorld();
	if (!World) return;

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

	RenderFrame.ClearViewportResources();
	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	RenderFrame.SetCameraInfo(Camera);
	RenderFrame.SetRenderSettings(ViewMode, ShowFlags);
	RenderFrame.SetRenderOptions(Opts);
	RenderFrame.SetViewportInfo(VP);
	RenderFrame.ViewportType = Opts.ViewportType;
	RenderFrame.OcclusionCulling = &GPUOcclusion;
	RenderFrame.LODContext = World->PrepareLODContext();

	if (const auto* ViewModePassRegistry = Renderer.GetViewModePassRegistry();
		ViewModePassRegistry && ViewModePassRegistry->HasConfig(ViewMode))
	{
		Renderer.SetActiveViewMode(ViewMode);
		if (Renderer.AcquireViewModeSurfaceSet(VP->GetWidth(), VP->GetHeight()) == nullptr)
		{
			Renderer.ClearActiveViewMode();
			Renderer.ReleaseViewModeSurfaceSet();
		}
	}
	else
	{
		Renderer.ClearActiveViewMode();
		Renderer.ReleaseViewModeSurfaceSet();
	}

	Renderer.BeginCollect(RenderFrame, Scene.GetProxyCount());
	FRenderPassContext PassContext = Renderer.CreatePassContext(RenderFrame, &Scene);

	{
		SCOPE_STAT_CAT("Collector", "3_Collect");

		RenderCollector.CollectWorld(World, RenderFrame, Scene, Renderer);
		RenderCollector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Scene);
		RenderCollector.CollectDebugDraw(RenderFrame, Scene);

		if (ShowFlags.bSceneOctree)
		{
			RenderCollector.CollectOctreeDebug(World->GetOctree(), Scene);
		}

		if (ShowFlags.bSceneBVH)
		{
			World->BuildWorldPrimitivePickingBVHNow();
			RenderCollector.CollectWorldBVHDebug(World->GetWorldPrimitivePickingBVH(), Scene);
		}

		if (ShowFlags.bWorldBound)
		{
			RenderCollector.CollectWorldBoundsDebug(RenderCollector.GetCollectedPrimitives().VisibleProxies, Scene);
		}

		if (VC == GetActiveViewport())
			RenderCollector.CollectOverlayText(GetOverlayStatSystem(), *this, Scene);
		RenderCollector.BuildFramePassCommands(RenderFrame, Scene, Renderer);

	}

	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		PassContext.VisibleProxies = &RenderCollector.GetCollectedPrimitives().VisibleProxies;
		Renderer.RunRootPipeline(ERenderPipelineType::EditorScene, PassContext);
	}

	if (GPUOcclusion.IsInitialized())
	{
		SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");

		GPUOcclusion.DispatchOcclusionTest(
			Ctx,
			VP->GetDepthCopySRV(),
			RenderCollector.GetCollectedPrimitives().VisibleProxies,
			RenderFrame.View, RenderFrame.Proj,
			VP->GetWidth(), VP->GetHeight());
	}
}
