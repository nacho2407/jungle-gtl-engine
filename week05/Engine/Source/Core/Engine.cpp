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
#include "Scene/Scene.h"
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

	// 1. 파생형이 내부 상태를 먼저 준비한다.
	PreInitialize();
	// 2. 엔진이 사용할 호스트 창 참조를 바인딩한다.
	BindHost(Args.MainWindow);

	// 3. 월드와 무관한 런타임 시스템을 먼저 초기화한다.
	if (!InitializeRuntimeSystems(Args.Hwnd, Args.Width, Args.Height))
	{
		return FailInitialize();
	}

	// 4. 시작 모드에 맞는 월드 컨텍스트와 월드를 생성한다.
	if (!InitializeWorlds(Args.Width, Args.Height))
	{
		return FailInitialize();
	}

	// 5. 메인 뷰포트 클라이언트를 만들어 기본 렌더링 경로를 준비한다.
	if (!InitializePrimaryViewport())
	{
		return FailInitialize();
	}

	// 6. 게임/에디터 전용 초기화는 파생형이 담당한다.
	if (!InitializeMode())
	{
		return FailInitialize();
	}

	// 7. 마지막 초기 검증이나 동기화를 수행한다.
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
	//TickWorlds(DeltaTime);
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

UScene* FEngine::GetScene() const
{
	return GetActiveScene();
}

UScene* FEngine::GetActiveScene() const
{
	const FWorldContext* Context = FindWorldContext(EWorldType::Game);
	return (Context && Context->World) ? Context->World->GetScene() : nullptr;
}

UScene* FEngine::GetGameScene() const
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

	// 런타임 시스템은 월드 생성 전에 모두 준비해 둔다.
	ObjManager = std::make_unique<FObjectManager>();

	FMaterialManager::Get().LoadAllMaterials(Renderer->GetDevice(), Renderer->GetRenderStateManager().get());

	InputManager = std::make_unique<FInputManager>();
	EnhancedInput = std::make_unique<FEnhancedInputManager>();
	PhysicsManager = std::make_unique<FPhysicsManager>();

	Timer.Initialize();
	RegisterConsoleVariables();

	return true;
}

bool FEngine::InitializeWorlds(int32 Width, int32 Height)
{
	// 월드는 런타임 시스템이 준비된 뒤에만 만든다.
	(void)Width;
	(void)Height;

	return true;
}

bool FEngine::InitializePrimaryViewport()
{
	// 기본 뷰포트는 월드 생성 이후에 만들어야 활성 씬과 바로 연결할 수 있다.
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

	CommandQueue.Clear();
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

void FEngine::DestroyWorldContext(FWorldContext* Context)
{
	if (!Context)
	{
		return;
	}

	if (Context->World)
	{
		Context->World->CleanupWorld();
		delete Context->World;
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

	//if (!PhysicsManager)
	//{
	//	return;
	//}

	//if (!WantsPhysicsDebugVisualization())
	//{
	//	return;
	//}

	//UScene* Scene = ActiveViewportClient ? ActiveViewportClient->ResolveScene(this) : GetActiveScene();
	//if (!Scene)
	//{
	//	return;
	//}

	//const FVector LineStart(2, 2, 0);
	//const FVector LineEnd(5, 5, 0);
	//FHitResult HitResult;

	//const bool bHit = PhysicsManager->Linetrace(Scene, LineStart, LineEnd, HitResult);
	//if (bHit && !HitResult.HitActor->IsA(ASkySphereActor::StaticClass()))
	//{
	//	for (UActorComponent* ActorComp : HitResult.HitActor->GetComponents())
	//	{
	//		if (!ActorComp->IsA(UPrimitiveComponent::StaticClass()))
	//		{
	//			continue;
	//		}

	//		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(ActorComp);
	//		if (!PrimitiveComponent->ShouldDrawDebugBounds())
	//		{
	//			continue;
	//		}

	//		const FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();
	//		DebugDrawManager.DrawCube(Bounds.Center, Bounds.BoxExtent, FVector4(1, 0, 0, 1));
	//	}

	//	DebugDrawManager.DrawCube(HitResult.HitLocation, FVector(0.1f, 0.1f, 0.1f), FVector4(0, 1, 0, 1));
	//}

	//if (Renderer)
	//{
	//	DebugDrawManager.DrawLine(LineStart, LineEnd, FVector4(0, 1, 1, 1));
	//}
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
		const FMatrix ViewInverse = CommandQueue.ViewMatrix.GetInverse();
		const FVector CameraPosition = ViewInverse.GetTranslation();
		ActiveViewportClient->BuildRenderCommands(this, Scene, Frustum, FShowFlags{}, CameraPosition, CommandQueue);
	}

	Renderer->SubmitCommands(CommandQueue);
	Renderer->ExecuteCommands();

	DebugDrawManager.Flush(Renderer.get(), FShowFlags{}, ActiveWorld);
	Renderer->EndFrame();
}

void FEngine::SyncPlatformState()
{
}

void FEngine::FinalizeFrame(float DeltaTime)
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
			OutResult = "ForceGC: FObjectManager is not available.";
		}
	}, "Force immediate garbage collection");
}
