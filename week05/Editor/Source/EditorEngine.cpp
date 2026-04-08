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
	if (IsPIEActive() || IsPIEPaused()) EndPIE();
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
	if (!bIsPIEActive)
	{
		StartPIE();
		return;
	}

	if (bIsPIEPaused)
	{
		TogglePIEPause();
	}
}

void FEditorEngine::PauseSimulation()
{
	if (bIsPIEActive && !bIsPIEPaused)
	{
		TogglePIEPause();
	}
}

void FEditorEngine::StopSimulation()
{
	if (bIsPIEActive)
	{
		EndPIE();
	}
}

FEditorEngine::ESimulationPlaybackState FEditorEngine::GetSimulationPlaybackState() const
{
	if (!bIsPIEActive)
	{
		return ESimulationPlaybackState::Stopped;
	}

	return bIsPIEPaused ? ESimulationPlaybackState::Paused : ESimulationPlaybackState::Playing;
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
	ImGui_ImplWin32_EnableDpiAwareness();

	FEngineLog::Get().SetCallback([this](const char* Msg)
	{
		EditorUI.GetConsole().AddLog("%s", Msg);
	});
}

void FEditorEngine::BindHost(FWindowsWindow* InMainWindow)
{
	MainWindow = InMainWindow;
	EditorUI.SetupWindow(InMainWindow);
}

bool FEditorEngine::InitializeWorlds()
{
	return InitEditorWorlds();
}

bool FEditorEngine::InitializeMode()
{
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

	SlateApplication = std::make_unique<FSlateApplication>();
	SlateApplication->Initialize(FRect(0, 0, W, H), VPs, MAX_VIEWPORTS);
	EditorUI.OnSlateReady();
	CreateInitUI();
	FObjManager::PreloadAllModelFiles(FPaths::FromPath(FPaths::MeshDir()).c_str());
}

void FEditorEngine::PrepareFrame(float DeltaTime)
{
	if (bIsPIEActive && PIEViewportId != INVALID_VIEWPORT_ID)
	{
		if (SlateApplication && !SlateApplication->IsViewportActive(PIEViewportId))
		{
			EndPIE();
		}
	}

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

	if (UWorld* ActiveWorld = GetActiveWorld())
	{
		ActiveWorld->Tick(DeltaTime);
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
	SyncPIECursorState();
}

FEditorViewportController* FEditorEngine::GetViewportController()
{
	return CameraSubsystem.GetViewportController();
}

void FEditorEngine::BuildDebugLineRenderRequest(const FShowFlags& ShowFlags, FDebugLineRenderRequest& OutRequest)
{
	OutRequest.Clear();

	if (ShowFlags.HasFlag(EEngineShowFlags::SF_DebugDraw))
	{
		AppendSelectedBVH(OutRequest);
	}

	if (UWorld* EditorWorld = GetEditorWorld())
	{
		GetDebugDrawManager().BuildRenderRequest(ShowFlags, EditorWorld, OutRequest);
	}
}

void FEditorEngine::AppendSelectedBVH(FDebugLineRenderRequest& InOutRequest) const
{
	AActor* SelectedActor = GetSelectedActor();
	if (!SelectedActor)
	{
		return;
	}

	UWorld* World = GetActiveWorld();
	if (!World)
	{
		return;
	}

	UStaticMeshComponent* MeshComp = nullptr;
	for (UActorComponent* Comp : SelectedActor->GetComponents())
	{
		if (Comp && Comp->IsA(UStaticMeshComponent::StaticClass()))
		{
			MeshComp = static_cast<UStaticMeshComponent*>(Comp);
			break;
		}
	}

	if (!MeshComp)
	{
		return;
	}

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
	if (!StaticMesh)
	{
		return;
	}

	const FMatrix& LocalToWorld = MeshComp->GetWorldTransform();
	StaticMesh->VisitMeshBVHNodes([&InOutRequest, &LocalToWorld](const FAABB& LocalBounds, int32 Depth, bool bIsLeaf)
		{
			const FVector& PMin = LocalBounds.PMin;
			const FVector& PMax = LocalBounds.PMax;
			const FVector Corners[8] = {
				{ PMin.X, PMin.Y, PMin.Z }, { PMax.X, PMin.Y, PMin.Z },
				{ PMin.X, PMax.Y, PMin.Z }, { PMax.X, PMax.Y, PMin.Z },
				{ PMin.X, PMin.Y, PMax.Z }, { PMax.X, PMin.Y, PMax.Z },
				{ PMin.X, PMax.Y, PMax.Z }, { PMax.X, PMax.Y, PMax.Z },
			};

			FVector WorldMin = LocalToWorld.TransformPosition(Corners[0]);
			FVector WorldMax = WorldMin;
			for (int32 Index = 1; Index < 8; ++Index)
			{
				const FVector W = LocalToWorld.TransformPosition(Corners[Index]);
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
		PIEWorld->MarkPendingKill();
		return false;
	}

	UpdateWorldAspectRatio(PIEWorld, AspectRatio);

	SavedPIEViewportStates.clear();
	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		if (!Entry.bActive)
		{
			continue;
		}

		FPIEViewportStateBackup Backup;
		Backup.ViewportId = Entry.Id;
		Backup.WorldContext = Entry.WorldContext;
		Backup.LocalState = Entry.LocalState;
		Backup.LocalState.ViewMode = Entry.LocalState.ViewMode;
		Backup.LocalState.ShowFlags = Entry.LocalState.ShowFlags;
		SavedPIEViewportStates.push_back(Backup);
	}

	SavedPIESelectedActor = GetSelectedActor();

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
		PIEViewportEntry->WorldContext = PIEWorldContext;
		PIEViewportId = PIEViewportEntry->Id;
		PIEViewportEntry->LocalState.ViewMode = ERenderMode::Lighting;
		PIEViewportEntry->LocalState.ShowFlags.SetFlag(EEngineShowFlags::SF_UUID, false);
		PIEViewportEntry->LocalState.ShowFlags.SetFlag(EEngineShowFlags::SF_DebugDraw, false);
		if (PIEViewportEntry->LocalState.ProjectionType == EViewportType::Perspective)
		{
			PIEViewportEntry->LocalState.Position = FVector::ZeroVector;
			PIEViewportEntry->LocalState.Rotation = FRotator::ZeroRotator;
			PIEViewportEntry->LocalState.bShowGrid = false;
		}
		else
		{
			// PIE should not inherit the last perspective camera when launched from
			// an ortho viewport. Start from a deterministic perspective state instead.
			const FViewportLocalState PreviousViewportState = PIEViewportEntry->LocalState;
			FViewportLocalState PIEViewportState = FViewportLocalState::CreateDefault(EViewportType::Perspective);
			PIEViewportState.ShowFlags = PreviousViewportState.ShowFlags;
			PIEViewportState.ViewMode = PreviousViewportState.ViewMode;
			PIEViewportState.GridSize = PreviousViewportState.GridSize;
			PIEViewportState.LineThickness = PreviousViewportState.LineThickness;
			PIEViewportState.NearPlane = PreviousViewportState.NearPlane;
			PIEViewportState.FarPlane = PreviousViewportState.FarPlane;
			PIEViewportState.bShowGrid = false;
			PIEViewportEntry->LocalState = PIEViewportState;
		}
	}

	SetSelectedActor(nullptr);

	PrePIEActiveWorldContext = ActiveWorldContext ? ActiveWorldContext : EditorWorldContext;
	ActiveWorldContext = PIEWorldContext;
	bIsPIEActive = true;
	bIsPIEPaused = false;
	bIsPIEInputCaptured = true;
	bWasCursorHiddenForPIE = true;
	bIsPIECursorCurrentlyHidden = false;
	CenterCursorInPIEViewport();
	SyncPIECursorState();

	PIEWorld->BeginPlay();
	return true;
}

void FEditorEngine::EndPIE()
{
	if (!bIsPIEActive)
	{
		return;
	}

	for (const FPIEViewportStateBackup& Backup : SavedPIEViewportStates)
	{
		FViewportEntry* RestoreViewportEntry = ViewportRegistry.FindEntryByViewportID(Backup.ViewportId);
		if (RestoreViewportEntry)
		{
			RestoreViewportEntry->WorldContext = Backup.WorldContext;
			RestoreViewportEntry->LocalState = Backup.LocalState;
		}
	}
	SavedPIEViewportStates.clear();

	DestroyWorldContext(PIEWorldContext);
	PIEWorldContext = nullptr;

	ActiveWorldContext = PrePIEActiveWorldContext ? PrePIEActiveWorldContext : EditorWorldContext;
	PrePIEActiveWorldContext = nullptr;
	SetSelectedActor(SavedPIESelectedActor.Get());
	SavedPIESelectedActor = nullptr;

	if (bWasCursorHiddenForPIE)
	{
		if (bIsPIECursorCurrentlyHidden)
		{
			::ShowCursor(TRUE);
		}
		bWasCursorHiddenForPIE = false;
		bIsPIECursorCurrentlyHidden = false;
	}
	::ClipCursor(nullptr);

	bIsPIEActive = false;
	bIsPIEPaused = false;
	bIsPIEInputCaptured = false;
	PIEViewportId = INVALID_VIEWPORT_ID;
}

void FEditorEngine::TogglePIEPause()
{
	if (bIsPIEActive)
	{
		bIsPIEPaused = !bIsPIEPaused;
	}
}

void FEditorEngine::CapturePIEInput()
{
	if (!bIsPIEActive || bIsPIEPaused || PIEViewportId == INVALID_VIEWPORT_ID)
	{
		return;
	}

	bIsPIEInputCaptured = true;
	bWasCursorHiddenForPIE = true;
	CenterCursorInPIEViewport();
	SyncPIECursorState();
}

void FEditorEngine::ReleasePIEInputCapture()
{
	if (!bIsPIEActive)
	{
		return;
	}

	bIsPIEInputCaptured = false;
	SyncPIECursorState();
}

bool FEditorEngine::InitEditorPreview()
{
	InitializeDefaultPreviewScene(this);
	PreviewViewportClient = std::make_unique<FPreviewViewportClient>(EditorUI, PreviewSceneContextName);
	return PreviewViewportClient != nullptr;
}

void FEditorEngine::InitEditorConsole()
{
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
	return CameraSubsystem.Initialize(GetActiveWorld(), GetInputManager(), GetEnhancedInputManager());
}

void FEditorEngine::InitEditorViewportRouting()
{
	SyncViewportClient();

	FViewportEntry* PerspEntry = nullptr;
	if (SlateApplication)
	{
		const FViewportId FocusedId = SlateApplication->GetFocusedViewportId();
		if (FocusedId != INVALID_VIEWPORT_ID)
		{
			FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
			if (FocusedEntry && FocusedEntry->bActive && FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
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

	for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
	{
		Entry.WorldContext = EditorWorldContext;
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
	if (FocusedEntry && FocusedEntry->bActive)
	{
		if (FocusedEntry->WorldContext && FocusedEntry->WorldContext->World)
		{
			ActiveWorldContext = FocusedEntry->WorldContext;
		}

		if (FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
		{
			LocalState = &FocusedEntry->LocalState;
		}
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

void FEditorEngine::SyncPIECursorState()
{
	if (!bIsPIEActive || PIEViewportId == INVALID_VIEWPORT_ID || MainWindow == nullptr)
	{
		if (bWasCursorHiddenForPIE && bIsPIECursorCurrentlyHidden)
		{
			::ShowCursor(TRUE);
			bIsPIECursorCurrentlyHidden = false;
		}
		::ClipCursor(nullptr);
		return;
	}

	if (!bIsPIEInputCaptured)
	{
		if (bWasCursorHiddenForPIE && bIsPIECursorCurrentlyHidden)
		{
			::ShowCursor(TRUE);
			bIsPIECursorCurrentlyHidden = false;
		}
		::ClipCursor(nullptr);
		return;
	}

	HWND Hwnd = MainWindow->GetHwnd();
	const bool bWindowActive = (Hwnd != nullptr) &&
		(::GetForegroundWindow() == Hwnd);

	if (!bWindowActive)
	{
		if (bWasCursorHiddenForPIE && bIsPIECursorCurrentlyHidden)
		{
			::ShowCursor(TRUE);
			bIsPIECursorCurrentlyHidden = false;
		}
		::ClipCursor(nullptr);
		return;
	}

	if (bWasCursorHiddenForPIE && !bIsPIECursorCurrentlyHidden)
	{
		::ShowCursor(FALSE);
		bIsPIECursorCurrentlyHidden = true;
	}

	FViewport* PIEViewport = FindViewport(PIEViewportId);
	if (PIEViewport == nullptr)
	{
		::ClipCursor(nullptr);
		return;
	}

	const FRect& Rect = PIEViewport->GetRect();
	if (!Rect.IsValid())
	{
		::ClipCursor(nullptr);
		return;
	}

	if (Hwnd == nullptr)
	{
		::ClipCursor(nullptr);
		return;
	}

	POINT TopLeft = { Rect.X, Rect.Y };
	POINT BottomRight = { Rect.X + Rect.Width, Rect.Y + Rect.Height };
	::ClientToScreen(Hwnd, &TopLeft);
	::ClientToScreen(Hwnd, &BottomRight);

	RECT ClipRect = { TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y };
	::ClipCursor(&ClipRect);
}

void FEditorEngine::CenterCursorInPIEViewport()
{
	if (!bIsPIEActive || PIEViewportId == INVALID_VIEWPORT_ID || MainWindow == nullptr)
	{
		return;
	}

	FViewport* PIEViewport = FindViewport(PIEViewportId);
	if (PIEViewport == nullptr)
	{
		return;
	}

	HWND Hwnd = MainWindow->GetHwnd();
	if (Hwnd == nullptr)
	{
		return;
	}

	const FRect& Rect = PIEViewport->GetRect();
	if (!Rect.IsValid())
	{
		return;
	}

	POINT Center = { Rect.X + Rect.Width / 2, Rect.Y + Rect.Height / 2 };
	::ClientToScreen(Hwnd, &Center);
	::SetCursorPos(Center.x, Center.y);
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
