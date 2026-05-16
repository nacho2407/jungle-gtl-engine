#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Paths.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Renderer/DefaultRenderPipeline.h"
#include "Camera/ViewportCamera.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Runtime/Script/ScriptManager.h"
#include "Serialization/SceneSaveManager.h"
#include "Editor/Selection/SelectionManager.h"

#include <filesystem>

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
	while (!WorldList.empty())
	{
		DestroyWorldContext(WorldList.back().ContextHandle);
	}

	RmlUiSystem.Shutdown();
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
	InputSystem::Get().Tick();
	WorldTick(DeltaTime);
	ProcessPendingSceneOpen();
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

bool UEngine::RequestOpenScene(const FString& ScenePath)
{
	const FString ResolvedPath = ResolveScenePath(ScenePath);
	if (ResolvedPath.empty())
	{
		return false;
	}

	if (!std::filesystem::exists(FPaths::ToWide(ResolvedPath)))
	{
		UE_LOG_ERROR("[Scene] Scene file missing: %s", ResolvedPath.c_str());
		return false;
	}

	PendingSceneOpenPath = ResolvedPath;
	bPendingSceneOpen = true;
	UE_LOG("[Scene] Queued scene open: %s", PendingSceneOpenPath.c_str());
	return true;
}

bool UEngine::RequestReloadScene()
{
	if (CurrentScenePath.empty())
	{
		return false;
	}

	return RequestOpenScene(CurrentScenePath);
}

void UEngine::WorldTick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->Tick(DeltaTime);
		if (FViewportCamera* ActiveCamera = World->GetActiveCamera())
		{
			AudioSystem.SetListener(
				ActiveCamera->GetLocation(),
				ActiveCamera->GetEffectiveForward(),
				ActiveCamera->GetEffectiveUp());
		}
	}
}

void UEngine::ProcessPendingSceneOpen()
{
	if (!bPendingSceneOpen)
	{
		return;
	}

	const FString ScenePath = PendingSceneOpenPath;
	bPendingSceneOpen = false;
	PendingSceneOpenPath.clear();

	if (!OpenSceneNow(ScenePath))
	{
		UE_LOG_ERROR("[Scene] Failed to open scene: %s", ScenePath.c_str());
	}
}

bool UEngine::OpenSceneNow(const FString& ScenePath)
{
	FWorldContext* ActiveContext = GetWorldContextFromHandle(ActiveWorldHandle);
	if (!ActiveContext)
	{
		UE_LOG_ERROR("[Scene] Cannot open scene without an active world context.");
		return false;
	}

	if (ScenePath.empty() || !std::filesystem::exists(FPaths::ToWide(ScenePath)))
	{
		UE_LOG_ERROR("[Scene] Scene file missing: %s", ScenePath.c_str());
		return false;
	}

	FWorldContext LoadedContext;
	FSceneSaveManager::Load(ScenePath, LoadedContext, nullptr);
	if (!LoadedContext.World)
	{
		return false;
	}

	UWorld* OldWorld = ActiveContext->World;
	const EWorldType TargetWorldType = ActiveContext->WorldType;
	const FName TargetHandle = ActiveContext->ContextHandle;
	const FString TargetName = ActiveContext->ContextName;

	OnSceneWorldWillUnload(OldWorld);
	if (OldWorld)
	{
		OldWorld->EndPlay(EEndPlayReason::Type::LevelTransition);
	}
	if (TargetWorldType == EWorldType::Game || TargetWorldType == EWorldType::PIE)
	{
		FScriptManager::Get().ResetLuaState();
	}

	LoadedContext.World->SetWorldType(TargetWorldType);
	LoadedContext.World->SyncSpatialIndex();

	ActiveContext->WorldType = TargetWorldType;
	ActiveContext->World = LoadedContext.World;
	ActiveContext->ContextHandle = TargetHandle;
	ActiveContext->ContextName = TargetName;
	ActiveContext->bPaused = false;
	SetActiveWorld(TargetHandle);
	CurrentScenePath = ScenePath;

	OnSceneWorldLoaded(ActiveContext->World);

	if (TargetWorldType == EWorldType::Game || TargetWorldType == EWorldType::PIE)
	{
		ActiveContext->World->BeginPlay();
	}

	if (OldWorld)
	{
		UObjectManager::Get().DestroyObject(OldWorld);
	}

	return true;
}

FString UEngine::ResolveScenePath(const FString& ScenePath) const
{
	if (ScenePath.empty())
	{
		return {};
	}

	return FPaths::Normalize(FPaths::ToAbsoluteString(FPaths::ToWide(ScenePath)));
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
    Context.SelectionManager = new FSelectionManager;
    Context.SelectionManager->Init();
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
            it->SelectionManager->Shutdown();
            delete it->SelectionManager;
            it->SelectionManager = nullptr;
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

FRuntimeInputPermissions UEngine::BuildRuntimeInputPermissions(
	const FGuiInputState& GuiState,
	bool bRuntimeUIConsumedInput) const
{
	FRuntimeInputPermissions Permissions;
	const bool bGameOnlyLocked =
		RuntimeInputMode == ERuntimeInputMode::GameOnly && bRuntimeCursorLocked;

	Permissions.bAllowRuntimeUIInput = !bGameOnlyLocked;
	Permissions.bAllowPlayerInput =
		RuntimeInputMode != ERuntimeInputMode::UIOnly && !bRuntimeUIConsumedInput;

	if (RuntimeInputMode == ERuntimeInputMode::UIOnly)
	{
		Permissions.bAllowPlayerInput = false;
		Permissions.bAllowLuaKeyboardInput = false;
		Permissions.bAllowLuaMouseInput = false;
		return Permissions;
	}

	if (RuntimeInputMode == ERuntimeInputMode::GameOnly)
	{
		Permissions.bAllowLuaKeyboardInput = bRuntimeCursorLocked;
		Permissions.bAllowLuaMouseInput = bRuntimeCursorLocked;
		return Permissions;
	}

	Permissions.bAllowLuaKeyboardInput = !(GuiState.bUsingKeyboard || GuiState.bUsingTextInput);
	Permissions.bAllowLuaMouseInput = !(GuiState.bUsingMouse || GuiState.bBlockViewportMouse);
	return Permissions;
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
