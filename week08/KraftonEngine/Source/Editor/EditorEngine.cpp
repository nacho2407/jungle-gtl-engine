#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"

IMPLEMENT_CLASS(UEditorEngine, UEngine)

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);

	FObjManager::ScanMeshAssets();
	FObjManager::ScanMaterialAssets();

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

	// Editor render pipeline
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
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

	// Ctrl+D — 선택된 액터 복제
	if (InputSystem::Get().GetKey(VK_CONTROL) && InputSystem::Get().GetKeyDown('D'))
	{
		const TArray<AActor*> ToDuplicate = SelectionManager.GetSelectedActors();
		if (!ToDuplicate.empty())
		{
			TArray<AActor*> NewSelection;
			for (AActor* Src : ToDuplicate)
			{
				if (!Src) continue;
				AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
				if (Dup)
				{
					NewSelection.push_back(Dup);
				}
			}
			SelectionManager.ClearSelection();
			for (AActor* Actor : NewSelection)
			{
				SelectionManager.ToggleSelect(Actor);
			}
		}
	}

	MainPanel.Update();
	UEngine::Tick(DeltaTime);
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
	FPlayInEditorSessionInfo Info;
	Info.OriginalRequestParams = Params;
	Info.PIEStartTime = 0.0;
	Info.PreviousActiveWorldHandle = GetActiveWorldHandle();
	PlayInEditorSessionInfo = Info;

	// 4) ActiveWorldHandle을 PIE로 전환 — 이후 GetWorld()는 PIE 월드를 반환.
	SetActiveWorld(FName("PIE"));

	// 5) 활성 뷰포트 카메라를 PIE 월드의 ActiveCamera로 설정 —
	//    UWorld::UpdateVisibleProxies가 ActiveCamera를 기준으로 frustum culling을 수행하므로
	//    이를 설정하지 않으면 PIE 월드의 VisibleProxies가 비어 있어 아무것도 렌더되지 않음.
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
		{
			PIEWorld->SetActiveCamera(VCCamera);
		}
	}

	// 6) Selection을 PIE 월드 기준으로 재바인딩 — 에디터 액터를 가리킨 채로 두면
	//    픽킹(=PIE 월드) / outliner / outline 렌더가 모두 어긋난다.
	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(PIEWorld);

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

	// Selection을 에디터 월드로 복원 — PIE 액터는 곧 파괴되므로 먼저 비운다.
	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(GetWorld());

	// PIE WorldContext 제거 (DestroyWorldContext가 EndPlay + DestroyObject 수행).
	DestroyWorldContext(FName("PIE"));

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
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
		Pipeline->OnSceneCleared();

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;

	ViewportLayout.DestroyAllCameras();
}
