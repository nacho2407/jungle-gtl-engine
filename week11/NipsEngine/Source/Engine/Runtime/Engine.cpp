#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Paths.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Renderer/DefaultRenderPipeline.h"
#include "Camera/ViewportCamera.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"

#include <algorithm>

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();

	InputSystem::Get().SetOwnerWindow(Window->GetHWND());
	Renderer.Create(Window->GetHWND());
	AudioSystem.Initialize();

	FResourceManager::Get().LoadFromAssetDirectory(FPaths::ToUtf8(FPaths::AssetDirectoryPath()));

	Renderer.CreateResources();

	SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
}

void UEngine::Shutdown()
{
	AudioSystem.Shutdown();
	RenderPipeline.reset();
	FResourceManager::Get().ReleaseGPUResources();
	Renderer.Release();
}

void UEngine::BeginPlay()
{
	FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	if (Context && Context->World)
	{
		if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
		{
			Context->World->BeginPlay();
		}
	}
}

void UEngine::Tick(float DeltaTime)
{
	UpdateTimeState(DeltaTime);
	InputSystem::Get().Tick();
	WorldTick(DeltaTime);
	AudioSystem.Tick(DeltaTime);
	Render(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
	if (RenderPipeline)
	{
		SCOPE_STAT("UEngine::Render");
		RenderPipeline->Execute(DeltaTime, Renderer);
	}
}

void UEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline)
{
	RenderPipeline = std::move(InPipeline);
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	Renderer.InvalidateSceneFinalTargets();
	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

bool UEngine::RequestQuitGame()
{
#if WITH_EDITOR
	UE_LOG_WARNING("[Engine] RequestQuitGame is ignored in editor builds.");
	return false;
#else
	if (!Window || !Window->GetHWND())
	{
		return false;
	}

	::PostMessageW(Window->GetHWND(), WM_CLOSE, 0, 0);
	return true;
#endif
}

void UEngine::SetTimeScale(float InTimeScale)
{
	TimeScale = std::max(0.0f, InTimeScale);
}

void UEngine::UpdateTimeState(float DeltaTime)
{
	LastUnscaledDeltaTime = std::max(0.0f, DeltaTime);

	const UWorld* World = GetWorld();
	const bool bScaleWorldTime =
		World &&
		(World->GetWorldType() == EWorldType::Game ||
		 World->GetWorldType() == EWorldType::PIE);

	LastDeltaTime = bScaleWorldTime ? LastUnscaledDeltaTime * TimeScale : LastUnscaledDeltaTime;
	RealTimeSeconds += LastUnscaledDeltaTime;
	GameTimeSeconds += LastDeltaTime;
}

void UEngine::WorldTick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World)
	{
		const bool bScaleWorldTime =
			World->GetWorldType() == EWorldType::Game ||
			World->GetWorldType() == EWorldType::PIE;
		World->Tick(bScaleWorldTime ? DeltaTime * TimeScale : DeltaTime);
		if (FViewportCamera* ActiveCamera = World->GetActiveCamera())
		{
			AudioSystem.SetListener(
				ActiveCamera->GetLocation(),
				ActiveCamera->GetEffectiveForward(),
				ActiveCamera->GetEffectiveUp());
		}
	}
}

UWorld* UEngine::GetWorld() const
{
	const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	return Context ? Context->World : nullptr;
}

APlayerController* UEngine::GetPrimaryPlayerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Actor))
		{
			return PlayerController;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
	FWorldContext Context;
	Context.WorldType = Type;
	Context.ContextHandle = Handle;
	Context.ContextName = Name.empty() ? Handle.ToString() : Name;
	Context.World = UObjectManager::Get().CreateObject<UWorld>();
	if (Context.World)
	{
		Context.World->SetWorldType(Type);
	}
	WorldList.push_back(Context);
	return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
	for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
	{
		if (it->ContextHandle == Handle)
		{
			it->World->EndPlay(EEndPlayReason::Type::Destroyed);
			UObjectManager::Get().DestroyObject(it->World);
			WorldList.erase(it);
			return;
		}
	}
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
	for (const FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.World == World)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
	ActiveWorldHandle = Handle;
}

void UEngine::SetRuntimeInputMode(ERuntimeInputMode InMode)
{
	RuntimeInputMode = InMode;
	InputSystem& Input = InputSystem::Get();
	if (RuntimeInputMode == ERuntimeInputMode::GameOnly)
	{
		bRuntimeCursorVisible = false;
		bRuntimeCursorLocked = true;
		Input.SetCursorVisibility(false);
	}
	else
	{
		bRuntimeCursorVisible = true;
		bRuntimeCursorLocked = false;
		Input.SetUseRawMouse(false);
		Input.LockMouse(false);
		Input.SetCursorVisibility(true);
	}
}

void UEngine::SetRuntimeCursorVisible(bool bVisible)
{
	bRuntimeCursorVisible = bVisible;
	InputSystem& Input = InputSystem::Get();
	if (bVisible)
	{
		bRuntimeCursorLocked = false;
		Input.SetUseRawMouse(false);
		Input.LockMouse(false);
	}
	Input.SetCursorVisibility(bVisible);
}

void UEngine::SetRuntimeCursorLocked(bool bLocked)
{
	bRuntimeCursorLocked = bLocked;
	InputSystem& Input = InputSystem::Get();
	if (bLocked)
	{
		bRuntimeCursorVisible = false;
		Input.SetCursorVisibility(false);
	}
	else
	{
		Input.SetUseRawMouse(false);
		Input.LockMouse(false);
	}
}
