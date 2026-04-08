#include "EditorEngine.h"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Actor/Actor.h"
#include "Core/ShowFlags.h"
#include "Object/Class.h"
#include "Renderer/MeshData.h"
#include "Renderer/Renderer.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Asset/ObjManager.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Level/Level.h"
#include "Viewport/Viewport.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/PreviewViewportClient.h"
#include "World/World.h"
#include "Slate/EditorViewportOverlay.h"

namespace
{
	constexpr const char* PreviewSceneContextName = "PreviewScene";

	const TArray<FWorldContext*>& GetEmptyPreviewWorldContexts()
	{
		static TArray<FWorldContext*> EmptyPreviewWorldContexts;
		return EmptyPreviewWorldContexts;
	}

	void InitializeDefaultPreviewScene(FEditorEngine* Engine)
	{
		if (Engine == nullptr)
		{
			return;
		}

		FWorldContext* PreviewContext = Engine->CreatePreviewWorldContext(PreviewSceneContextName, 1280, 720);
		if (PreviewContext == nullptr || PreviewContext->World == nullptr)
		{
			return;
		}

		UWorld* PreviewWorld = PreviewContext->World;
		if (PreviewWorld->GetActors().empty())
		{
			/*AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>("PreviewCube");
			if (PreviewActor)
			{
				UStaticMeshComponent* PreviewComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(PreviewActor);
				PreviewActor->AddOwnedComponent(PreviewComponent);
				PreviewActor->SetRootComponent(PreviewComponent);

				PreviewComponent->SetStaticMesh(FObjManager::GetPrimitiveCube());
				PreviewActor->SetActorLocation({ 0.0f, 0.0f, 0.0f });
			}*/
		}

		if (UCameraComponent* PreviewCamera = PreviewWorld->GetActiveCameraComponent())
		{
			PreviewCamera->GetCamera()->SetPosition({ -8.0f, -8.0f, 6.0f });
			PreviewCamera->GetCamera()->SetRotation(45.0f, -20.0f);
			PreviewCamera->SetFov(50.0f);
		}
	}
}

FEditorEngine::~FEditorEngine() = default;

void FEditorEngine::Shutdown()
{
	FEngineLog::Get().SetCallback({});
	EditorUI.SaveEditorSettings();

	if (GetViewportClient() == PreviewViewportClient.get())
	{
		SetViewportClient(nullptr);
	}

	PreviewViewportClient.reset();
	CameraSubsystem.Shutdown();
	SelectionSubsystem.Shutdown();
	ReleaseEditorWorlds();

	FEngine::Shutdown();
}

void FEditorEngine::SetSelectedActor(AActor* InActor)
{
	SelectionSubsystem.SetSelectedActor(InActor);
}

AActor* FEditorEngine::GetSelectedActor() const
{
	return SelectionSubsystem.GetSelectedActor();
}

void FEditorEngine::ActivateEditorScene()
{
	ActiveWorldContext = (EditorWorldContext && EditorWorldContext->World) ? EditorWorldContext : nullptr;
}

bool FEditorEngine::ActivatePreviewScene(const FString& ContextName)
{
	FWorldContext* PreviewContext = FindPreviewWorld(ContextName);
	if (PreviewContext == nullptr)
	{
		return false;
	}

	ActiveWorldContext = PreviewContext;
	return true;
}

ULevel* FEditorEngine::GetEditorScene() const
{
	return (EditorWorldContext && EditorWorldContext->World) ? EditorWorldContext->World->GetScene() : nullptr;
}

ULevel* FEditorEngine::GetPreviewScene(const FString& ContextName) const
{
	const FWorldContext* Context = FindPreviewWorld(ContextName);
	return (Context && Context->World) ? Context->World->GetScene() : nullptr;
}

UWorld* FEditorEngine::GetEditorWorld() const
{
	return EditorWorldContext ? EditorWorldContext->World : nullptr;
}

const TArray<FWorldContext*>& FEditorEngine::GetPreviewWorldContexts() const
{
	return PreviewWorldContexts.empty() ? GetEmptyPreviewWorldContexts() : PreviewWorldContexts;
}

FWorldContext* FEditorEngine::CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height)
{
	if (ContextName.empty())
	{
		return nullptr;
	}

	if (FWorldContext* ExistingContext = FindPreviewWorld(ContextName))
	{
		return ExistingContext;
	}

	const float AspectRatio = (Height > 0) ? (static_cast<float>(Width) / static_cast<float>(Height)) : 1.0f;
	FWorldContext* PreviewContext = CreateWorldContext(ContextName, EWorldType::Preview, AspectRatio, false);
	if (!PreviewContext)
	{
		return nullptr;
	}

	PreviewWorldContexts.push_back(PreviewContext);
	return PreviewContext;
}

ULevel* FEditorEngine::GetScene() const
{
	return GetActiveScene();
}

ULevel* FEditorEngine::GetActiveScene() const
{
	UWorld* ActiveWorld = GetActiveWorld();
	return ActiveWorld ? ActiveWorld->GetScene() : nullptr;
}

UWorld* FEditorEngine::GetActiveWorld() const
{
	return ActiveWorldContext ? ActiveWorldContext->World : FEngine::GetActiveWorld();
}

const FWorldContext* FEditorEngine::GetActiveWorldContext() const
{
	return ActiveWorldContext ? ActiveWorldContext : FEngine::GetActiveWorldContext();
}

void FEditorEngine::HandleResize(int32 Width, int32 Height)
{
	FEngine::HandleResize(Width, Height);

	if (Width == 0 || Height == 0)
	{
		return;
	}

	UpdateEditorWorldAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
}

void FEditorEngine::PreInitialize()
{
	// 에디터 UI가 올라오기 전에 DPI와 로그 연결만 먼저 준비한다.
	ImGui_ImplWin32_EnableDpiAwareness();

	FEngineLog::Get().SetCallback([this](const char* Msg)
	{
		EditorUI.GetConsole().AddLog("%s", Msg);
	});
}

void FEditorEngine::BindHost(FWindowsWindow* InMainWindow)
{
	// 실제 UI/뷰포트 생성은 뒤 단계에서 하고, 여기서는 창 참조만 저장한다.
	MainWindow = InMainWindow;
	EditorUI.SetupWindow(InMainWindow);
}

bool FEditorEngine::InitializeWorlds()
{
	return InitEditorWorlds();
}

bool FEditorEngine::InitializeMode()
{
	// 에디터 전용 초기화는 규약상 이 단계에서만 수행한다.
	if (!InitEditorPreview())
	{
		return false;
	}

	InitEditorConsole();

	if (!InitEditorCamera())
	{
		return false;
	}

	InitEditorViewportRouting();
	return true;
}

void FEditorEngine::FinalizeInitialize()
{
	// 모드 전용 초기화가 모두 끝난 뒤 마지막 상태를 기록한다.
	UE_LOG("EditorEngine initialized");
	const int32 W = MainWindow ? MainWindow->GetWidth() : 800;
	const int32 H = MainWindow ? MainWindow->GetHeight() : 600;

	TArray<FViewport>& Viewports = ViewportRegistry.GetViewports();
	FViewport* VPs[MAX_VIEWPORTS] = {
		&Viewports[0], &Viewports[1], &Viewports[2], &Viewports[3]
	};
	SlateApplication = std::make_unique<FSlateApplication>();
	SlateApplication->Initialize(FRect(0, 0, W, H), VPs, MAX_VIEWPORTS);
	EditorUI.OnSlateReady();
	CreateInitUI();
	FObjManager::PreloadAllModelFiles(FPaths::FromPath(FPaths::MeshDir()).c_str());
}

void FEditorEngine::PrepareFrame(float DeltaTime)
{
	SyncViewportClient();
	SyncFocusedViewportLocalState();
	CameraSubsystem.PrepareFrame(GetActiveWorld(), GetScene(), DeltaTime);
}

void FEditorEngine::TickWorlds(float DeltaTime)
{
	if (bIsPIEActive)
	{
		if (bIsPIEPaused)
		{
			return;
		}

		if (PIEWorldContext && PIEWorldContext->World)
		{
			PIEWorldContext->World->Tick(DeltaTime);
		}
		return;
	}

	if (EditorWorldContext && EditorWorldContext->World)
	{
		EditorWorldContext->World->Tick(DeltaTime);
	}
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	auto Client = std::make_unique<FEditorViewportClient>(*this, EditorUI, ViewportRegistry, MainWindow);
	EditorViewportClientRaw = Client.get();
	return Client;
}

void FEditorEngine::RenderFrame()
{
	FRenderer* Renderer = GetRenderer();
	if (!Renderer || Renderer->IsOccluded())
	{
		return;
	}

	Renderer->BeginFrame();

	if (EditorViewportClientRaw)
	{
		EditorViewportClientRaw->Render(this, Renderer);
	}

	Renderer->EndFrame();
}

void FEditorEngine::SyncPlatformState()
{
	SyncPlatformCursor();
}

FEditorViewportController* FEditorEngine::GetViewportController()
{
	return CameraSubsystem.GetViewportController();
}

void FEditorEngine::FlushDebugDrawForViewport(FRenderer* Renderer, const FShowFlags& ShowFlags, bool bClearAfterFlush)
{
	if (!Renderer)
	{
		return;
	}

	if (ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw))
	{
		DrawSelectedBVH(Renderer);
	}

	if (UWorld* ActiveWorld = GetActiveWorld())
	{
		GetDebugDrawManager().Flush(Renderer, ShowFlags, ActiveWorld, bClearAfterFlush);
	}
	else if (bClearAfterFlush)
	{
		GetDebugDrawManager().Clear();
	}
}

void FEditorEngine::DrawSelectedBVH(FRenderer* Renderer)
{
	AActor* SelectedActor = GetSelectedActor();
	if (!SelectedActor) return;

	UWorld* World = GetActiveWorld();
	if (!World) return;

	// 선택된 액터의 StaticMeshComponent 탐색
	UStaticMeshComponent* MeshComp = nullptr;
	for (UActorComponent* Comp : SelectedActor->GetComponents())
	{
		if (Comp && Comp->IsA(UStaticMeshComponent::StaticClass()))
		{
			MeshComp = static_cast<UStaticMeshComponent*>(Comp);
			break;
		}
	}

	if (!MeshComp) return;

	// --- 씬 BVH: 선택된 컴포넌트까지의 경로만 표시 ---
	// 내부 노드: 초록, 리프: 노랑
	if (ULevel* Scene = World->GetScene())
	{
		Scene->VisitBVHNodesForPrimitive(MeshComp, [Renderer](const FAABB& Bounds, int32 Depth, bool bIsLeaf)
			{
				const FVector Center = (Bounds.PMin + Bounds.PMax) * 0.5f;
				const FVector Extent = (Bounds.PMax - Bounds.PMin) * 0.5f;
				const FVector4 Color = bIsLeaf
					? FVector4(1.0f, 1.0f, 0.0f, 1.0f)
					: FVector4(0.0f, 1.0f, 0.0f, 1.0f);
				Renderer->DrawCube(Center, Extent, Color);
			});
	}

	// --- 메시 BVH: 선택된 메시의 전체 트리를 재귀적으로 표시 ---
	// 내부 노드: 하늘색, 리프: 파랑
	UStaticMesh* StaticMesh = MeshComp->GetStaticMesh();
	if (!StaticMesh) return;

	const FMatrix& LocalToWorld = MeshComp->GetWorldTransform();
	StaticMesh->VisitMeshBVHNodes([Renderer, &LocalToWorld](const FAABB& LocalBounds, int32 Depth, bool bIsLeaf)
		{
			const FVector& PMin = LocalBounds.PMin;
			const FVector& PMax = LocalBounds.PMax;
			const FVector Corners[8] = {
				{PMin.X, PMin.Y, PMin.Z}, {PMax.X, PMin.Y, PMin.Z},
				{PMin.X, PMax.Y, PMin.Z}, {PMax.X, PMax.Y, PMin.Z},
				{PMin.X, PMin.Y, PMax.Z}, {PMax.X, PMin.Y, PMax.Z},
				{PMin.X, PMax.Y, PMax.Z}, {PMax.X, PMax.Y, PMax.Z},
			};
			FVector WorldMin = LocalToWorld.TransformPosition(Corners[0]);
			FVector WorldMax = WorldMin;
			for (int32 i = 1; i < 8; ++i)
			{
				const FVector W = LocalToWorld.TransformPosition(Corners[i]);
				WorldMin.X = (W.X < WorldMin.X) ? W.X : WorldMin.X;
				WorldMin.Y = (W.Y < WorldMin.Y) ? W.Y : WorldMin.Y;
				WorldMin.Z = (W.Z < WorldMin.Z) ? W.Z : WorldMin.Z;
				WorldMax.X = (W.X > WorldMax.X) ? W.X : WorldMax.X;
				WorldMax.Y = (W.Y > WorldMax.Y) ? W.Y : WorldMax.Y;
				WorldMax.Z = (W.Z > WorldMax.Z) ? W.Z : WorldMax.Z;
			}
			const FVector Center = (WorldMin + WorldMax) * 0.5f;
			const FVector Extent = (WorldMax - WorldMin) * 0.5f;
			const FVector4 Color = bIsLeaf
				? FVector4(0.0f, 0.5f, 1.0f, 1.0f)
				: FVector4(0.0f, 1.0f, 1.0f, 1.0f);
			Renderer->DrawCube(Center, Extent, Color);
		});
}

void FEditorEngine::ClearDebugDrawForFrame()
{
	GetDebugDrawManager().Clear();
}

void FEditorEngine::CreateInitUI()
{
	auto* RawEditorVP = static_cast<FEditorViewportClient*>(ViewportClient.get());
	std::unique_ptr<SEditorViewportOverlay> Overlay = std::make_unique<SEditorViewportOverlay>(this, &EditorUI, RawEditorVP);
	SWidget* RawOverlay = SlateApplication->CreateWidget(std::move(Overlay));
	SlateApplication->AddOverlayWidget(RawOverlay);
}

bool FEditorEngine::StartPIE()
{
	if (bIsPIEActive)
	{
		return false;
	}

	if (EditorWorldContext == nullptr || EditorWorldContext->World == nullptr)
	{
		return false;
	}

	// 월드 복사 및 PIE 월드 컨텍스트 생성
	UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorldContext->World);
	if (PIEWorld == nullptr)
	{
		return false;
	}

	PIEWorld->ResetRuntimeState();

	const float AspectRatio = GetWindowAspectRatio();
	PIEWorldContext = CreateWorldContext("PIE", EWorldType::PIE, PIEWorld);
	if (PIEWorldContext == nullptr)
	{
		PIEWorld->CleanupWorld();
		delete PIEWorld;
		return false;
	}

	UpdateWorldAspectRatio(PIEWorld, AspectRatio);

	// 나중에 PIE 종료 시점에 복원할 수 있도록 PIE 시작 시점의 뷰포트 상태 저장
	SavedPIEViewportStates.clear();
	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		if (!Entry.bActive)
		{
			continue;
		}

		FPIEViewportStateBackup Backup;
		Backup.ViewportId = Entry.Id;
		Backup.LocalState = Entry.LocalState;
		SavedPIEViewportStates.push_back(Backup);
	}

	SavedPIESelectedActor = GetSelectedActor();

	// PIE 시작 시점에 포커스된 뷰포트가 있으면 해당 뷰포트를 계속 사용, 없으면 활성 뷰포트 중 첫 번째를 사용
	// [Minjun] 카메라 관련 로직은 손 볼 필요가 있음
	FViewportEntry* PIEViewportEntry = nullptr;
	if (SlateApplication)
	{
		const FViewportId FocusedId = SlateApplication->GetFocusedViewportId();
		if (FocusedId != INVALID_VIEWPORT_ID)
		{
			FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
			if (FocusedEntry && FocusedEntry->bActive)
			{
				PIEViewportEntry = FocusedEntry;
			}
		}
	}
	if (PIEViewportEntry == nullptr)
	{
		for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
		{
			if (Entry.bActive)
			{
				PIEViewportEntry = &Entry;
				break;
			}
		}
	}
	if (PIEViewportEntry)
	{
		// PIE 모드에서는 항상 원근 뷰포트로 시작하도록 강제합니다. 나중에 뷰포트가 포커스될 때 저장된 LocalState로 복원할 수 있도록 합니다.
		PIEViewportEntry->LocalState.ProjectionType = EViewportType::Perspective;

		// [Minjun][임시 조치] PIE 시작 시점의 카메라는 위치 (0, 0, 0), 회전 (0,0,0)으로 초기화
		PIEViewportEntry->LocalState.Position = FVector::ZeroVector;
		PIEViewportEntry->LocalState.Rotation = FRotator::ZeroRotator;
	}

	// PIE 모드에서는 그리드가 항상 꺼진 상태로 시작하도록 강제
	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		if (!Entry.bActive)
		{
			continue;
		}

		Entry.LocalState.bShowGrid = false;
	}

	SetSelectedActor(nullptr);

	// PIE 월드 컨텍스트 활성화 및 BeginPlay 호출
	ActiveWorldContext = PIEWorldContext;

	PIEWorld->BeginPlay();

	// PIE 모드 진입 시 마우스 커서 숨김
	// [Minjun] 나중에 UI 상에서 PIE 모드 진입 / 종료 지점이 생기면 아래 코드 활성화할 것
	//::ShowCursor(FALSE);
	//bWasCursorHiddenForPIE = true;

	bIsPIEActive = true;
	bIsPIEPaused = false;

	return true;
}

void FEditorEngine::EndPIE()
{
	if (!bIsPIEActive)
	{
		return;
	}

	// 저장해둔 뷰포트 상태 복원
	for (const FPIEViewportStateBackup& Backup : SavedPIEViewportStates)
	{
		FViewportEntry* RestoreViewportEntry = ViewportRegistry.FindEntryByViewportID(Backup.ViewportId);
		if (RestoreViewportEntry)
		{
			RestoreViewportEntry->LocalState = Backup.LocalState;
		}
	}
	SavedPIEViewportStates.clear();

	// PIE 월드 정리 및 컨텍스트 제거
	if (PIEWorldContext && PIEWorldContext->World)
	{
		PIEWorldContext->World->EndPlay();
		PIEWorldContext->World->CleanupWorld();
	}

	DestroyWorldContext(PIEWorldContext);
	PIEWorldContext = nullptr;

	// 에디터 월드 컨텍스트로 복원
	ActiveWorldContext = EditorWorldContext;
	SetSelectedActor(SavedPIESelectedActor.Get());
	SavedPIESelectedActor = nullptr;

	if (bWasCursorHiddenForPIE)
	{
		::ShowCursor(TRUE);
		bWasCursorHiddenForPIE = false;
	}

	bIsPIEActive = false;
	bIsPIEPaused = false;
}

void FEditorEngine::TogglePIEPause()
{
	if (bIsPIEActive)
	{
		bIsPIEPaused = !bIsPIEPaused;
	}
}

bool FEditorEngine::InitEditorPreview()
{
	// 에디터가 항상 접근 가능한 기본 프리뷰 월드와 프리뷰 뷰포트를 준비한다.
	InitializeDefaultPreviewScene(this);
	PreviewViewportClient = std::make_unique<FPreviewViewportClient>(EditorUI, PreviewSceneContextName);
	return PreviewViewportClient != nullptr;
}

void FEditorEngine::InitEditorConsole()
{
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	// 현재 등록된 콘솔 변수/명령을 UI 자동완성 목록에 반영한다.
	CVM.GetAllNames([this](const FString& Name)
	{
		EditorUI.GetConsole().RegisterCommand(Name.c_str());
	});

	EditorUI.GetConsole().SetCommandHandler([](const char* CommandLine)
	{
		FString Result;
		if (FConsoleVariableManager::Get().Execute(CommandLine, Result))
		{
			FEngineLog::Get().Log("%s", Result.c_str());
		}
		else
		{
			FEngineLog::Get().Log("[error] Unknown command: '%s'", CommandLine);
		}
	});
}

bool FEditorEngine::InitEditorCamera()
{
	// 에디터 카메라는 월드가 준비된 뒤에만 생성할 수 있다.
	return CameraSubsystem.Initialize(GetActiveWorld(), GetInputManager(), GetEnhancedInputManager());
}

void FEditorEngine::InitEditorViewportRouting()
{
	// 초기 활성 월드가 Editor/Preview 중 무엇인지에 따라 적절한 뷰포트를 고른다.
	SyncViewportClient();

	// Perspective Entry의 LocalState를 입력 컨트롤러에 연결
	FViewportEntry* PerspEntry = nullptr;
	if (SlateApplication)
	{
		const FViewportId FocusedId = SlateApplication->GetFocusedViewportId();
		if (FocusedId != INVALID_VIEWPORT_ID)
		{
			FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
			if (FocusedEntry &&
				FocusedEntry->bActive &&
				FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
			{
				PerspEntry = FocusedEntry;
			}
		}
	}
	if (!PerspEntry)
	{
		PerspEntry = ViewportRegistry.FindEntryByType(EViewportType::Perspective);
	}
	if (PerspEntry)
	{
		CameraSubsystem.GetViewportController()->SetActiveLocalState(&PerspEntry->LocalState);
	}
}

bool FEditorEngine::InitEditorWorlds()
{
	const float AspectRatio = GetWindowAspectRatio();

	EditorWorldContext = CreateWorldContext("EditorScene", EWorldType::Editor, AspectRatio, true);
	if (!EditorWorldContext)
	{
		return false;
	}

	ActivateEditorScene();
	return true;
}

void FEditorEngine::ReleaseEditorWorlds()
{
	ActiveWorldContext = nullptr;

	for (FWorldContext* PreviewContext : PreviewWorldContexts)
	{
		DestroyWorldContext(PreviewContext);
	}
	PreviewWorldContexts.clear();

	DestroyWorldContext(EditorWorldContext);
	EditorWorldContext = nullptr;
}

FWorldContext* FEditorEngine::FindPreviewWorld(const FString& ContextName)
{
	for (FWorldContext* Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context;
		}
	}

	return nullptr;
}

const FWorldContext* FEditorEngine::FindPreviewWorld(const FString& ContextName) const
{
	for (const FWorldContext* Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context;
		}
	}

	return nullptr;
}

void FEditorEngine::UpdateEditorWorldAspectRatio(float AspectRatio)
{
	UpdateWorldAspectRatio(EditorWorldContext ? EditorWorldContext->World : nullptr, AspectRatio);

	for (FWorldContext* PreviewContext : PreviewWorldContexts)
	{
		UpdateWorldAspectRatio(PreviewContext ? PreviewContext->World : nullptr, AspectRatio);
	}
}

void FEditorEngine::SyncFocusedViewportLocalState()
{
	if (!EditorViewportClientRaw || !SlateApplication)
	{
		return;
	}

	FViewportId FocusedId = SlateApplication->GetFocusedViewportId();
	FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
	FViewportLocalState* LocalState = nullptr;
	if (FocusedEntry && FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
	{
		LocalState = &FocusedEntry->LocalState;
	}

	CameraSubsystem.GetViewportController()->SetActiveLocalState(LocalState);
}

void FEditorEngine::SyncPlatformCursor()
{
	if (!SlateApplication || !SlateApplication->GetIsCoursorInArea())
	{
		return;
	}

	const EMouseCursor SlateCursor = SlateApplication->GetCurrentCursor();
	LPCWSTR WinCursorName = IDC_ARROW;
	switch (SlateCursor)
	{
	case EMouseCursor::Default:         WinCursorName = IDC_ARROW;  break;
	case EMouseCursor::ResizeLeftRight: WinCursorName = IDC_SIZEWE; break;
	case EMouseCursor::ResizeUpDown:    WinCursorName = IDC_SIZENS; break;
	case EMouseCursor::Hand:            WinCursorName = IDC_HAND;   break;
	case EMouseCursor::None:            WinCursorName = nullptr;    break;
	}

	if (WinCursorName)
	{
		::SetCursor(::LoadCursor(NULL, WinCursorName));
	}
	else
	{
		::SetCursor(nullptr);
	}
}

void FEditorEngine::SyncViewportClient()
{
	if (!GetActiveWorldContext())
	{
		return;
	}

	IViewportClient* TargetViewportClient = ViewportClient.get();
	const FWorldContext* ActiveSceneContext = GetActiveWorldContext();
	if (ActiveSceneContext && ActiveSceneContext->WorldType == EWorldType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (GetViewportClient() != TargetViewportClient)
	{
		SetViewportClient(TargetViewportClient);
	}
}

FViewport* FEditorEngine::FindViewport(FViewportId Id)
{
	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		if (Entry.Id == Id && Entry.bActive)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}
