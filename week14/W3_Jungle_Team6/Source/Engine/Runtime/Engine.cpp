#include "Engine/Runtime/Engine.h"

#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderCollector.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();

	Renderer.Create(Window->GetHWND());
	FRenderCollector::Initialize(Renderer.GetFD3DDevice().GetDevice());
	FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::ResourceFilePath()));
}

void UEngine::Shutdown()
{
	FResourceManager::Get().ReleaseGPUResources();
	FRenderCollector::Release();
	Renderer.Release();
}

void UEngine::BeginPlay()
{
	if (!Scene.empty() && Scene[CurrentWorld])
	{
		Scene[CurrentWorld]->BeginPlay();
	}
}

void UEngine::Tick(float DeltaTime)
{
	InputSystem::Update();
	UpdateWorld(DeltaTime);
	Render(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
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

void UEngine::UpdateWorld(float DeltaTime)
{
	if (!Scene.empty() && Scene[CurrentWorld])
	{
		Scene[CurrentWorld]->Tick(DeltaTime);
	}
}

