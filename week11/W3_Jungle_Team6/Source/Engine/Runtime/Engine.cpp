#include "Engine/Runtime/Engine.h"

#include "Engine/Core/InputSystem.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderCollector.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(HWND InHWindow)
{
	HWindow = InHWindow;

	RECT Rect;
	GetClientRect(HWindow, &Rect);
	WindowWidth = static_cast<float>(Rect.right - Rect.left);
	WindowHeight = static_cast<float>(Rect.bottom - Rect.top);

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();

	Renderer.Create(HWindow);
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

void UEngine::BeginFrame(float DeltaTime)
{
	InputSystem::Update();
}

void UEngine::Tick(float DeltaTime)
{
	SyncCameraFromRenderHandler();
	UpdateWorld(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
	Renderer.BeginFrame();
	Renderer.EndFrame();
}

void UEngine::EndFrame()
{
	UObjectManager::Get().CollectGarbage();
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	WindowWidth = static_cast<float>(Width);
	WindowHeight = static_cast<float>(Height);
	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void UEngine::UpdateWorld(float DeltaTime)
{
	if (!Scene.empty() && Scene[CurrentWorld])
	{
		Scene[CurrentWorld]->Tick(DeltaTime);
	}
}

void UEngine::SyncCameraFromRenderHandler()
{
	if (Camera)
	{
		Camera->ApplyCameraState();
	}
}
