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
			// 프리뷰 월드는 에디터가 열리자마자 바로 보일 수 있도록 기본 카메라를 잡아둔다.
			PreviewCamera->GetCamera()->SetPosition({ -8.0f, -8.0f, 6.0f });
			PreviewCamera->GetCamera()->SetRotation(45.0f, -20.0f);
			PreviewCamera->SetFov(50.0f);
		}
	}
}

FEditorEngine::~FEditorEngine() = default;

void FEditorEngine::Shutdown()
{
	// 에디터 로그/설정/UI와 월드 컨텍스트를 먼저 정리한 뒤 기반 엔진을 종료한다.
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

void FEditorEngine::PlaySimulation()
{
	SimulationPlaybackState = ESimulationPlaybackState::Playing;
}

void FEditorEngine::PauseSimulation()
{
	SimulationPlaybackState = ESimulationPlaybackState::Paused;
}

void FEditorEngine::StopSimulation()
{
	SimulationPlaybackState = ESimulationPlaybackState::Stopped;
}

void FEditorEngine::ActivateEditorScene()
{
	// 에디터 모드의 기본 활성 월드는 메인 에디터 월드다.
	ActiveEditorWorldContext = (EditorWorldContext && EditorWorldContext->World) ? EditorWorldContext : nullptr;
}

bool FEditorEngine::ActivatePreviewScene(const FString& ContextName)
{
	FWorldContext* PreviewContext = FindPreviewWorld(ContextName);
	if (PreviewContext == nullptr)
	{
		return false;
	}

	ActiveEditorWorldContext = PreviewContext;
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
	return ActiveEditorWorldContext ? ActiveEditorWorldContext->World : FEngine::GetActiveWorld();
}

const FWorldContext* FEditorEngine::GetActiveWorldContext() const
{
	return ActiveEditorWorldContext ? ActiveEditorWorldContext : FEngine::GetActiveWorldContext();
}

void FEditorEngine::HandleResize(int32 Width, int32 Height)
{
	FEngine::HandleResize(Width, Height);

	if (Width == 0 || Height == 0)
	{
		return;
	}

	// 에디터는 메인 월드뿐 아니라 모든 프리뷰 월드도 같은 종횡비 업데이트가 필요하다.
	UpdateEditorWorldAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
}

void FEditorEngine::PreInitialize()
{
	// 에디터 시작 전 로그를 UI 콘솔로 보내고, ImGui DPI 설정을 먼저 적용한다.
	ImGui_ImplWin32_EnableDpiAwareness();

	FEngineLog::Get().SetCallback([this](const char* Msg)
	{
		EditorUI.GetConsole().AddLog("%s", Msg);
	});
}

void FEditorEngine::BindHost(FWindowsWindow* InMainWindow)
{
	MainWindow = InMainWindow;
	// EditorUI가 윈도우 핸들과 크기 정보를 참조할 수 있게 미리 연결한다.
	EditorUI.SetupWindow(InMainWindow);
}

bool FEditorEngine::InitializeWorlds(int32 Width, int32 Height)
{
	return InitEditorWorlds(Width, Height);
}

bool FEditorEngine::InitializeMode()
{
	// 에디터 모드 초기화는 프리뷰 -> 콘솔 -> 카메라 -> 뷰포트 라우팅 순서로 진행한다.
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
	UE_LOG("EditorEngine initialized");
	const int32 W = MainWindow ? MainWindow->GetWidth() : 800;
	const int32 H = MainWindow ? MainWindow->GetHeight() : 600;

	TArray<FViewport>& Viewports = ViewportRegistry.GetViewports();
	FViewport* VPs[MAX_VIEWPORTS] = {
		&Viewports[0], &Viewports[1], &Viewports[2], &Viewports[3]
	};
	// Slate가 실제 뷰포트 레이아웃과 오버레이를 구성한 뒤 초기 UI를 올린다.
	SlateApplication = std::make_unique<FSlateApplication>();
	SlateApplication->Initialize(FRect(0, 0, W, H), VPs, MAX_VIEWPORTS);
	EditorUI.OnSlateReady();
	CreateInitUI();
	FObjManager::PreloadAllModelFiles(FPaths::FromPath(FPaths::MeshDir()).c_str());
}

void FEditorEngine::PrepareFrame(float DeltaTime)
{
	// 포커스된 뷰포트에 맞는 입력/카메라 대상 상태를 매 프레임 동기화한다.
	SyncViewportClient();
	SyncFocusedViewportLocalState();
	CameraSubsystem.PrepareFrame(GetActiveWorld(), GetScene(), DeltaTime);
}

void FEditorEngine::TickWorlds(float DeltaTime)
{
	if (SimulationPlaybackState != ESimulationPlaybackState::Playing)
	{
		return;
	}

	if (UWorld* ActiveWorld = GetActiveWorld())
	{
		ActiveWorld->Tick(DeltaTime);
	}
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	// 에디터 메인 뷰포트 클라이언트는 이후 서비스 객체들의 허브 역할을 한다.
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

	// 에디터 프레임은 백버퍼 시작 -> ImGui 프레임 -> 뷰포트 렌더 -> UI 마감 순서로 흐른다.
	Renderer->BeginFrame();
	EditorUI.BeginFrame();

	if (EditorViewportClientRaw)
	{
		EditorViewportClientRaw->Render(this, Renderer);
	}

	EditorUI.EndFrame();
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

void FEditorEngine::BuildDebugLineRenderRequest(const FShowFlags& ShowFlags, FDebugLineRenderRequest& OutRequest)
{
	// 에디터 디버그 라인은 선택 BVH와 엔진 공통 디버그 도형을 합쳐 하나의 request로 만든다.
	OutRequest.Clear();

	if (ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw))
	{
		AppendSelectedBVH(OutRequest);
	}

	if (UWorld* ActiveWorld = GetActiveWorld())
	{
		GetDebugDrawManager().BuildRenderRequest(ShowFlags, ActiveWorld, OutRequest);
	}
}

void FEditorEngine::AppendSelectedBVH(FDebugLineRenderRequest& InOutRequest) const
{
	// 선택된 액터가 스태틱 메시일 때만 씬 BVH와 메시 BVH를 시각화한다.
	AActor* SelectedActor = GetSelectedActor();
	if (!SelectedActor) return;

	UWorld* World = GetActiveWorld();
	if (!World) return;
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
	if (ULevel* Scene = World->GetScene())
	{
		Scene->VisitBVHNodesForPrimitive(MeshComp, [&InOutRequest](const FAABB& Bounds, int32 Depth, bool bIsLeaf)
			{
				const FVector Center = (Bounds.PMin + Bounds.PMax) * 0.5f;
				const FVector Extent = (Bounds.PMax - Bounds.PMin) * 0.5f;
				const FVector4 Color = bIsLeaf
					? FVector4(1.0f, 1.0f, 0.0f, 1.0f)
					: FVector4(0.0f, 1.0f, 0.0f, 1.0f);
				FDebugLineRenderFeature::AddCube(InOutRequest, Center, Extent, Color);
			});
	}
	UStaticMesh* StaticMesh = MeshComp->GetStaticMesh();
	if (!StaticMesh) return;

	const FMatrix& LocalToWorld = MeshComp->GetWorldTransform();
	StaticMesh->VisitMeshBVHNodes([&InOutRequest, &LocalToWorld](const FAABB& LocalBounds, int32 Depth, bool bIsLeaf)
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
			FDebugLineRenderFeature::AddCube(InOutRequest, Center, Extent, Color);
		});
}

void FEditorEngine::ClearDebugDrawForFrame()
{
	GetDebugDrawManager().Clear();
}

void FEditorEngine::CreateInitUI()
{
	// 에디터 시작 시 뷰포트 오버레이를 Slate 오버레이 레이어에 연결한다.
	auto* RawEditorVP = static_cast<FEditorViewportClient*>(ViewportClient.get());
	std::unique_ptr<SEditorViewportOverlay> Overlay = std::make_unique<SEditorViewportOverlay>(this, &EditorUI, RawEditorVP);
	SWidget* RawOverlay = SlateApplication->CreateWidget(std::move(Overlay));
	SlateApplication->AddOverlayWidget(RawOverlay);
}

bool FEditorEngine::InitEditorPreview()
{
	// 프리뷰 월드는 에셋 미리보기나 보조 화면이 필요할 때 사용할 별도 월드다.
	InitializeDefaultPreviewScene(this);
	PreviewViewportClient = std::make_unique<FPreviewViewportClient>(EditorUI, PreviewSceneContextName);
	return PreviewViewportClient != nullptr;
}

void FEditorEngine::InitEditorConsole()
{
	// 등록된 콘솔 변수 이름을 UI 콘솔 자동완성과 실행 경로에 연결한다.
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();
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
	// 에디터 카메라는 활성 월드와 입력 시스템을 기반으로 동작한다.
	return CameraSubsystem.Initialize(GetActiveWorld(), GetInputManager(), GetEnhancedInputManager());
}

void FEditorEngine::InitEditorViewportRouting()
{
	// 포커스된 perspective 뷰포트를 카메라 조작 대상 로컬 상태로 연결한다.
	SyncViewportClient();
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

bool FEditorEngine::InitEditorWorlds(int32 Width, int32 Height)
{
	const float AspectRatio = (Height > 0)
		? (static_cast<float>(Width) / static_cast<float>(Height))
		: 1.0f;

	// 에디터 메인 월드는 게임 월드와 별도로 독립적인 Editor 타입 컨텍스트를 사용한다.
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
	// 프리뷰 월드를 먼저 비우고 마지막에 에디터 메인 월드를 정리한다.
	ActiveEditorWorldContext = nullptr;

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
	// 메인 에디터 월드와 프리뷰 월드는 같은 창 크기 기준 종횡비를 공유한다.
	UpdateWorldAspectRatio(EditorWorldContext ? EditorWorldContext->World : nullptr, AspectRatio);

	for (FWorldContext* PreviewContext : PreviewWorldContexts)
	{
		UpdateWorldAspectRatio(PreviewContext ? PreviewContext->World : nullptr, AspectRatio);
	}
}

void FEditorEngine::SyncFocusedViewportLocalState()
{
	// 포커스된 perspective 뷰포트만 카메라 컨트롤러의 대상 로컬 상태가 된다.
	if (!EditorViewportClientRaw || !SlateApplication)
	{
		return;
	}

	FViewportId FocusedId = SlateApplication->GetFocusedViewportId();
	FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
	FViewportLocalState* LocalState = nullptr;
	if (FocusedEntry && FocusedEntry->bActive && FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
	{
		LocalState = &FocusedEntry->LocalState;
	}

	CameraSubsystem.GetViewportController()->SetActiveLocalState(LocalState);
}

void FEditorEngine::SyncPlatformCursor()
{
	// Slate가 계산한 커서 모양을 실제 OS 커서에 반영한다.
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
	// 활성 월드가 프리뷰면 PreviewViewportClient, 아니면 에디터 메인 ViewportClient를 사용한다.
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
