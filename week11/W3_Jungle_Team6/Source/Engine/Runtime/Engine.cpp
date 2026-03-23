#include "Engine/Runtime/Engine.h"

#include "Core/Paths.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderCollector.h"
#include "GameFramework/World.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();

	Renderer.Create(Window->GetHWND());
	RenderCollector.Initialize(Renderer.GetFD3DDevice().GetDevice());
	FResourceManager::Get().LoadFromFile(
		FPaths::ToUtf8(FPaths::ResourceFilePath()), Renderer.GetFD3DDevice().GetDevice());
}

void UEngine::Shutdown()
{
	FResourceManager::Get().ReleaseGPUResources();
	RenderCollector.Release();
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
	//TODO: See UEditorEngine::Render() for how to use RenderBus and render passes.
	Renderer.BeginFrame();
	Renderer.EndFrame();
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
			it->World->EndPlay();
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

