#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Paths.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Renderer/DefaultRenderPipeline.h"
#include "GameFramework/World.h"

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

	FResourceManager::Get().LoadFromAssetDirectory(
		FPaths::ToUtf8(FPaths::AssetDirectoryPath()), Renderer.GetFD3DDevice().GetDevice());
	
	// FResourceManager::Get().LoadFromFile(
	// 	FPaths::ToUtf8(FPaths::ResourceFilePath()), Renderer.GetFD3DDevice().GetDevice());


	SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
}

void UEngine::Shutdown()
{
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

	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void UEngine::WorldTick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->Tick(DeltaTime);
	}
}

UWorld* UEngine::GetWorld() const
{
	const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
	FWorldContext Context;
	Context.WorldType = Type;
	Context.ContextHandle = Handle;
	Context.ContextName = Name.empty() ? Handle.ToString() : Name;
	Context.World = UObjectManager::Get().CreateObject<UWorld>();
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
