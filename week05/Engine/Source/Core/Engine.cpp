#include "Engine.h"

#include "Platform/Windows/WindowsWindow.h"
#include "Actor/Actor.h"
#include "Actor/SkySphereActor.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputManager.h"
#include "Math/Frustum.h"
#include "Object/ObjectFactory.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectManager.h"
#include "Physics/PhysicsManager.h"
#include "Primitive/PrimitiveBase.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Scene.h"
#include "ViewportClient.h"
#include "World/World.h"
#include "World/WorldManager.h"

FEngine* GEngine = nullptr;

namespace
{
	const TArray<std::unique_ptr<FEditorWorldContext>>& GetEmptyPreviewWorldContexts()
	{
		static TArray<std::unique_ptr<FEditorWorldContext>> EmptyPreviewWorldContexts;
		return EmptyPreviewWorldContexts;
	}

	const FTimer& GetEmptyTimer()
	{
		static FTimer EmptyTimer;
		return EmptyTimer;
	}
}

FEngine::~FEngine()
{
	Shutdown();
}

bool FEngine::Initialize(const FEngineInitArgs& Args)
{
	if (!Args.Hwnd)
	{
		return false;
	}

	GEngine = this;

	PreInitialize();
	OnHostWindowReady(Args.MainWindow);

	if (!InitializeRuntime(Args.Hwnd, Args.Width, Args.Height, GetStartupSceneType()))
	{
		return false;
	}

	ViewportClient = CreateViewportClient();
	SetViewportClient(ViewportClient.get());

	PostInitialize();

	return true;
}

void FEngine::TickFrame()
{
	if (!Renderer)
	{
		return;
	}

	BeginFrame();

	const float DeltaTime = GetDeltaTime();

	Tick(DeltaTime);
	TickInput(DeltaTime);
	TickPhysics(DeltaTime);
	TickWorld(DeltaTime);
	RenderFrame();
	RunLateUpdate(DeltaTime);
}

CRenderer* FEngine::GetRenderer() const
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

CInputManager* FEngine::GetInputManager() const
{
	return InputManager;
}

CEnhancedInputManager* FEngine::GetEnhancedInputManager() const
{
	return EnhancedInput;
}

const FTimer& FEngine::GetTimer() const
{
	return Renderer ? Timer : GetEmptyTimer();
}

float FEngine::GetDeltaTime() const
{
	return Renderer ? Timer.GetDeltaTime() : 0.0f;
}

FWorldManager* FEngine::GetWorldManager() const
{
	return WorldManager.get();
}

UScene* FEngine::GetScene() const
{
	return WorldManager ? WorldManager->GetActiveScene() : nullptr;
}

UScene* FEngine::GetActiveScene() const
{
	return WorldManager ? WorldManager->GetActiveScene() : nullptr;
}

UScene* FEngine::GetEditorScene() const
{
	return WorldManager ? WorldManager->GetEditorScene() : nullptr;
}

UScene* FEngine::GetGameScene() const
{
	return WorldManager ? WorldManager->GetGameScene() : nullptr;
}

UScene* FEngine::GetPreviewScene(const FString& ContextName) const
{
	return WorldManager ? WorldManager->GetPreviewScene(ContextName) : nullptr;
}

void FEngine::SetSelectedActor(AActor* InActor)
{
	(void)InActor;
}

AActor* FEngine::GetSelectedActor() const
{
	return nullptr;
}

void FEngine::ActivateEditorScene() const
{
	if (WorldManager)
	{
		WorldManager->ActivateEditorScene();
	}
}

void FEngine::ActivateGameScene() const
{
	if (WorldManager)
	{
		WorldManager->ActivateGameScene();
	}
}

bool FEngine::ActivatePreviewScene(const FString& ContextName) const
{
	return WorldManager ? WorldManager->ActivatePreviewScene(ContextName) : false;
}

UWorld* FEngine::GetActiveWorld() const
{
	return WorldManager ? WorldManager->GetActiveWorld() : nullptr;
}

UWorld* FEngine::GetEditorWorld() const
{
	return WorldManager ? WorldManager->GetEditorWorld() : nullptr;
}

UWorld* FEngine::GetGameWorld() const
{
	return WorldManager ? WorldManager->GetGameWorld() : nullptr;
}

const FWorldContext* FEngine::GetActiveWorldContext() const
{
	return WorldManager ? WorldManager->GetActiveWorldContext() : nullptr;
}

const TArray<std::unique_ptr<FEditorWorldContext>>& FEngine::GetPreviewWorldContexts() const
{
	return WorldManager ? WorldManager->GetPreviewWorldContexts() : GetEmptyPreviewWorldContexts();
}

FEditorWorldContext* FEngine::CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height)
{
	return WorldManager ? WorldManager->CreatePreviewWorldContext(ContextName, Width, Height) : nullptr;
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

	if (WorldManager)
	{
		WorldManager->OnResize(Width, Height);
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

bool FEngine::InitializeRuntime(HWND Hwnd, int32 Width, int32 Height, ESceneType StartupSceneType)
{
	FPaths::Initialize();

	WindowWidth = Width;
	WindowHeight = Height;

	Renderer = std::make_unique<CRenderer>(Hwnd, Width, Height);
	if (!Renderer)
	{
		return false;
	}

	ObjManager = new ObjectManager();

	FMaterialManager::Get().LoadAllMaterials(Renderer->GetDevice(), Renderer->GetRenderStateManager().get());

	InputManager = new CInputManager();
	EnhancedInput = new CEnhancedInputManager();
	PhysicsManager = std::make_unique<CPhysicsManager>();

	Timer.Initialize();
	RegisterConsoleVariables();

	WorldManager = std::make_unique<FWorldManager>();
	const float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	if (!WorldManager->Initialize(AspectRatio, StartupSceneType, Renderer.get()))
	{
		ReleaseRuntime();
		return false;
	}

	return true;
}

void FEngine::ReleaseRuntime()
{
	if (WorldManager)
	{
		WorldManager->Release();
		WorldManager.reset();
	}

	if (ObjManager)
	{
		ObjManager->FlushKilledObjects();
		delete ObjManager;
		ObjManager = nullptr;
	}

	delete EnhancedInput;
	EnhancedInput = nullptr;

	delete InputManager;
	InputManager = nullptr;

	PhysicsManager.reset();
	CPrimitiveBase::ClearCache();
	Renderer.reset();

	CommandQueue.Clear();
	LastGCTime = 0.0;
}

void FEngine::BeginFrame()
{
	Timer.Tick();
}

void FEngine::TickInput(float DeltaTime)
{
	if (InputManager)
	{
		InputManager->Tick();
	}

	if (EnhancedInput && InputManager)
	{
		EnhancedInput->ProcessInput(InputManager, DeltaTime);
	}

	if (ActiveViewportClient)
	{
		ActiveViewportClient->Tick(this, DeltaTime);
	}
}

void FEngine::TickPhysics(float DeltaTime)
{
	if (!PhysicsManager)
	{
		return;
	}

	UScene* Scene = ActiveViewportClient ? ActiveViewportClient->ResolveScene(this) : GetActiveScene();
	if (!Scene)
	{
		return;
	}

	FVector LineStart(2, 2, 0);
	FVector LineEnd(5, 5, 0);
	FHitResult HitResult;

	const bool bHit = PhysicsManager->Linetrace(Scene, LineStart, LineEnd, HitResult);
	if (bHit && !HitResult.HitActor->IsA(ASkySphereActor::StaticClass()))
	{
		for (UActorComponent* ActorComp : HitResult.HitActor->GetComponents())
		{
			if (!ActorComp->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(ActorComp);
			if (!PrimitiveComponent->ShouldDrawDebugBounds())
			{
				continue;
			}

			FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();
			DebugDrawManager.DrawCube(Bounds.Center, Bounds.BoxExtent, FVector4(1, 0, 0, 1));
		}

		DebugDrawManager.DrawCube(HitResult.HitLocation, FVector(0.1f, 0.1f, 0.1f), FVector4(0, 1, 0, 1));
	}

	if (Renderer)
	{
		DebugDrawManager.DrawLine(LineStart, LineEnd, FVector4(0, 1, 1, 1));
	}
}

void FEngine::TickWorld(float DeltaTime)
{
	if (UWorld* World = GetActiveWorld())
	{
		World->Tick(DeltaTime);
	}
}

void FEngine::RenderFrame()
{
	UScene* Scene = ActiveViewportClient ? ActiveViewportClient->ResolveScene(this) : GetActiveScene();
	if (!Renderer || !Scene || Renderer->IsOccluded())
	{
		return;
	}

	Renderer->BeginFrame();

	UWorld* ActiveWorld = GetActiveWorld();
	if (!ActiveWorld)
	{
		Renderer->EndFrame();
		return;
	}

	UCameraComponent* ActiveCamera = ActiveWorld->GetActiveCameraComponent();
	if (!ActiveCamera)
	{
		Renderer->EndFrame();
		return;
	}

	CommandQueue.Clear();
	CommandQueue.Reserve(Renderer->GetPrevCommandCount());
	CommandQueue.ViewMatrix = ActiveCamera->GetViewMatrix();
	CommandQueue.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

	FFrustum Frustum;
	const FMatrix ViewProjection = CommandQueue.ViewMatrix * CommandQueue.ProjectionMatrix;
	Frustum.ExtractFromVP(ViewProjection);

	if (ActiveViewportClient)
	{
		ActiveViewportClient->BuildRenderCommands(this, Scene, Frustum, CommandQueue);
	}

	Renderer->SubmitCommands(CommandQueue);
	Renderer->ExecuteCommands();

	const FShowFlags& ShowFlags = ActiveViewportClient ? ActiveViewportClient->GetShowFlags() : FShowFlags();
	DebugDrawManager.Flush(Renderer.get(), ShowFlags, ActiveWorld);
	Renderer->EndFrame();
}

void FEngine::RunLateUpdate(float DeltaTime)
{
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
			OutResult = "ForceGC: ObjectManager is not available.";
		}
	}, "Force immediate garbage collection");
}
