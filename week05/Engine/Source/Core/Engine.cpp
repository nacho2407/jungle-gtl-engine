#include "Engine.h"

#include "Platform/Windows/WindowsWindow.h"
#include "Asset/ObjManager.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputManager.h"
#include "Math/Frustum.h"
#include "Object/ObjectManager.h"
#include "Physics/PhysicsManager.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Level/Level.h"
#include "ViewportClient.h"
#include "World/World.h"
#include "Object/ObjectFactory.h"
#include "Primitive/PrimitiveGizmo.h"

FEngine* GEngine = nullptr;

namespace
{
	const FTimer& GetEmptyTimer()
	{
		static FTimer EmptyTimer;
		return EmptyTimer;
	}
}

FEngine::~FEngine() = default;
FEngine::FEngine() = default;

bool FEngine::Initialize(const FEngineInitArgs& Args)
{
	if (!Args.Hwnd)
	{
		return false;
	}

	auto FailInitialize = [this]() -> bool
	{
		Shutdown();
		return false;
	};

	GEngine = this;
	PreInitialize();
	BindHost(Args.MainWindow);

	if (!InitializeRuntimeSystems(Args.Hwnd, Args.Width, Args.Height))
	{
		return FailInitialize();
	}

	if (!InitializeWorlds())
	{
		return FailInitialize();
	}

	if (!InitializePrimaryViewport())
	{
		return FailInitialize();
	}

	if (!InitializeMode())
	{
		return FailInitialize();
	}

	FinalizeInitialize();
	return true;
}

void FEngine::Tick()
{
	if (!Renderer)
	{
		return;
	}

	BeginFrame();

	const float DeltaTime = GetDeltaTime();
	PrepareFrame(DeltaTime);
	ProcessInput(DeltaTime);
	TickPhysics(DeltaTime);
	TickWorlds(DeltaTime);
	RenderFrame();
	SyncPlatformState();
	FinalizeFrame(DeltaTime);
}

void FEngine::PrepareFrame(float DeltaTime)
{
	(void)DeltaTime;
}

FRenderer* FEngine::GetRenderer() const
{
	return Renderer.get();
}

IViewportClient* FEngine::GetViewportClient() const
{
	return ActiveViewportClient;
}

void FEngine::SetViewportClient(IViewportClient* InViewportClient)
{
	if (ActiveViewportClient == InViewportClient)
	{
		return;
	}

	if (ActiveViewportClient && Renderer)
	{
		ActiveViewportClient->Detach(this, Renderer.get());
	}

	ActiveViewportClient = InViewportClient;

	if (ActiveViewportClient && Renderer)
	{
		ActiveViewportClient->Attach(this, Renderer.get());
	}
}

FInputManager* FEngine::GetInputManager() const
{
	return InputManager.get();
}

FEnhancedInputManager* FEngine::GetEnhancedInputManager() const
{
	return EnhancedInput.get();
}

const FTimer& FEngine::GetTimer() const
{
	return Renderer ? Timer : GetEmptyTimer();
}

float FEngine::GetDeltaTime() const
{
	return Renderer ? Timer.GetDeltaTime() : 0.0f;
}

ULevel* FEngine::GetScene() const
{
	return GetActiveScene();
}

ULevel* FEngine::GetActiveScene() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return (Context && Context->World) ? Context->World->GetScene() : nullptr;
}

ULevel* FEngine::GetGameScene() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return (Context && Context->World) ? Context->World->GetScene() : nullptr;
}

void FEngine::ActivateGameScene() const
{
}

UWorld* FEngine::GetActiveWorld() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return Context ? Context->World : nullptr;
}

UWorld* FEngine::GetGameWorld() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return Context ? Context->World : nullptr;
}

const FWorldContext* FEngine::GetActiveWorldContext() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return (Context && Context->IsValid()) ? Context : nullptr;
}

bool FEngine::HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (InputManager)
	{
		InputManager->ProcessMessage(Hwnd, Msg, WParam, LParam);
	}

	if (ActiveViewportClient)
	{
		ActiveViewportClient->HandleMessage(this, Hwnd, Msg, WParam, LParam);
	}

	return false;
}

void FEngine::HandleResize(int32 Width, int32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	WindowWidth = Width;
	WindowHeight = Height;

	if (Renderer)
	{
		Renderer->OnResize(Width, Height);
	}

	const float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	for (const std::unique_ptr<FWorldContext>& Context : WorldContexts)
	{
		UpdateWorldAspectRatio(Context ? Context->World : nullptr, AspectRatio);
	}
}

void FEngine::Shutdown()
{
	if (GEngine == this)
	{
		GEngine = nullptr;
	}

	SetViewportClient(nullptr);
	ReleaseRuntime();
	ViewportClient.reset();
}

bool FEngine::InitializeRuntimeSystems(HWND Hwnd, int32 Width, int32 Height)
{
	FPaths::Initialize();

	WindowWidth = Width;
	WindowHeight = Height;

	Renderer = std::make_unique<FRenderer>(Hwnd, Width, Height);
	if (!Renderer)
	{
		return false;
	}

	ObjManager = std::make_unique<FObjectManager>();
	FMaterialManager::Get().LoadAllMaterials(Renderer->GetDevice(), Renderer->GetRenderStateManager().get());

	InputManager = std::make_unique<FInputManager>();
	EnhancedInput = std::make_unique<FEnhancedInputManager>();
	PhysicsManager = std::make_unique<FPhysicsManager>();

	Timer.Initialize();
	RegisterConsoleVariables();
	return true;
}

bool FEngine::InitializeWorlds()
{
	return true;
}

bool FEngine::InitializePrimaryViewport()
{
	ViewportClient = CreateViewportClient();
	if (!ViewportClient)
	{
		return false;
	}

	SetViewportClient(ViewportClient.get());
	return true;
}

void FEngine::ReleaseRuntime()
{
	while (!WorldContexts.empty())
	{
		DestroyWorldContext(WorldContexts.back().get());
	}

	if (ObjManager)
	{
		FObjManager::ClearCache();
		ObjManager->FlushKilledObjects();
	}
	ObjManager.reset();

	EnhancedInput.reset();
	InputManager.reset();
	PhysicsManager.reset();
	FPrimitiveGizmo::ClearCache();
	Renderer.reset();

	LastGCTime = 0.0;
}

FWorldContext* FEngine::FindWorldContext(EWorldType WorldType)
{
	for (const std::unique_ptr<FWorldContext>& Context : WorldContexts)
	{
		if (Context && Context->WorldType == WorldType)
		{
			return Context.get();
		}
	}

	return nullptr;
}

const FWorldContext* FEngine::FindWorldContext(EWorldType WorldType) const
{
	for (const std::unique_ptr<FWorldContext>& Context : WorldContexts)
	{
		if (Context && Context->WorldType == WorldType)
		{
			return Context.get();
		}
	}

	return nullptr;
}

FWorldContext* FEngine::CreateWorldContext(const FString& ContextName, EWorldType WorldType, float AspectRatio, bool bDefaultScene)
{
	std::unique_ptr<FWorldContext> NewContext = std::make_unique<FWorldContext>();
	NewContext->ContextName = ContextName;
	NewContext->WorldType = WorldType;
	NewContext->World = FObjectFactory::ConstructObject<UWorld>(nullptr, ContextName);
	if (!NewContext->World)
	{
		return nullptr;
	}

	NewContext->World->SetWorldType(WorldType);
	if (bDefaultScene)
	{
		NewContext->World->InitializeWorld(AspectRatio, Renderer ? Renderer->GetDevice() : nullptr);
	}
	else
	{
		NewContext->World->InitializeWorld(AspectRatio);
	}

	FWorldContext* CreatedContext = NewContext.get();
	WorldContexts.push_back(std::move(NewContext));
	return CreatedContext;
}

FWorldContext* FEngine::CreateWorldContext(const FString& ContextName, EWorldType WorldType, UWorld* ExistingWorld)
{
	if (!ExistingWorld)
	{
		return nullptr;
	}

	std::unique_ptr<FWorldContext> NewContext = std::make_unique<FWorldContext>();
	NewContext->ContextName = ContextName;
	NewContext->WorldType = WorldType;
	NewContext->World = ExistingWorld;
	NewContext->World->SetWorldType(WorldType);

	FWorldContext* CreatedContext = NewContext.get();
	WorldContexts.push_back(std::move(NewContext));
	return CreatedContext;
}

void FEngine::DestroyWorldContext(FWorldContext* Context)
{
	if (!Context)
	{
		return;
	}

	if (Context->World)
	{
		Context->World->EndPlay();
		Context->World->CleanupWorld();
		Context->World->MarkPendingKill();
		// World 바로 제거
		ObjManager->FlushKilledObjects();
		Context->World = nullptr;
	}

	Context->Reset();

	for (auto It = WorldContexts.begin(); It != WorldContexts.end(); ++It)
	{
		if (It->get() == Context)
		{
			WorldContexts.erase(It);
			break;
		}
	}
}

void FEngine::UpdateWorldAspectRatio(UWorld* World, float AspectRatio) const
{
	if (World && World->GetCamera())
	{
		World->GetCamera()->SetAspectRatio(AspectRatio);
	}
}

void FEngine::BeginFrame()
{
	Timer.Tick();
}

void FEngine::ProcessInput(float DeltaTime)
{
	if (InputManager)
	{
		InputManager->Tick();
	}

	if (EnhancedInput && InputManager)
	{
		EnhancedInput->ProcessInput(InputManager.get(), DeltaTime);
	}

	if (ActiveViewportClient)
	{
		ActiveViewportClient->Tick(this, DeltaTime);
	}
}

void FEngine::TickPhysics(float DeltaTime)
{
	(void)DeltaTime;
}

void FEngine::RenderFrame()
{
	ULevel* Scene = ActiveViewportClient ? ActiveViewportClient->ResolveScene(this) : GetActiveScene();
	if (!Renderer || !Scene || Renderer->IsOccluded())
	{
		return;
	}

	Renderer->BeginFrame();

	if (ActiveViewportClient)
	{
		ActiveViewportClient->Render(this, Renderer.get());
	}

	FDebugLineRenderRequest DebugLineRequest;
	DebugDrawManager.BuildRenderRequest(FShowFlags{}, GetActiveWorld(), DebugLineRequest);
	Renderer->RenderDebugLines(DebugLineRequest);
	DebugDrawManager.Clear();
	Renderer->EndFrame();
}

void FEngine::SyncPlatformState()
{
}

void FEngine::FinalizeFrame(float DeltaTime)
{
	(void)DeltaTime;

	if (GCInterval <= 0.0 || !ObjManager)
	{
		return;
	}

	const double CurrentTime = Timer.GetTotalTime();
	if ((CurrentTime - LastGCTime) >= GCInterval)
	{
		ObjManager->FlushKilledObjects();
		LastGCTime = CurrentTime;
	}
}

void FEngine::RegisterConsoleVariables()
{
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	FConsoleVariable* MaxFPSVar = CVM.Find("t.MaxFPS");
	if (!MaxFPSVar)
	{
		MaxFPSVar = CVM.Register("t.MaxFPS", 0.0f, "Maximum FPS limit (0 = unlimited)");
	}
	MaxFPSVar->SetOnChanged([this](FConsoleVariable* Var)
	{
		Timer.SetMaxFPS(Var->GetFloat());
	});
	Timer.SetMaxFPS(MaxFPSVar->GetFloat());

	FConsoleVariable* VSyncVar = CVM.Find("r.VSync");
	if (!VSyncVar)
	{
		VSyncVar = CVM.Register("r.VSync", 0, "Enable VSync (0 = off, 1 = on)");
	}
	VSyncVar->SetOnChanged([this](FConsoleVariable* Var)
	{
		if (Renderer)
		{
			Renderer->SetVSync(Var->GetInt() != 0);
		}
	});
	if (Renderer)
	{
		Renderer->SetVSync(VSyncVar->GetInt() != 0);
	}

	FConsoleVariable* GCIntervalVar = CVM.Find("gc.Interval");
	if (!GCIntervalVar)
	{
		GCIntervalVar = CVM.Register("gc.Interval", 30.0f, "GC interval in seconds (0 = disabled)");
	}
	GCIntervalVar->SetOnChanged([this](FConsoleVariable* Var)
	{
		GCInterval = static_cast<double>(Var->GetFloat());
	});
	GCInterval = static_cast<double>(GCIntervalVar->GetFloat());

	CVM.RegisterCommand("ForceGC", [this](FString& OutResult)
	{
		if (ObjManager)
		{
			ObjManager->FlushKilledObjects();
			LastGCTime = Timer.GetTotalTime();
			OutResult = "ForceGC: Garbage collection completed.";
		}
		else
		{
			OutResult = "ForceGC: FObjectManager is not available.";
		}
	}, "Force immediate garbage collection");
}
