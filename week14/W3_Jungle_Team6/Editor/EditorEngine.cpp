#include "Editor/EditorEngine.h"

#include "Engine/Core/InputSystem.h"

#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderCollectorContext.h"

void FEditorEngine::Create(HWND InHWindow)
{
	HWindow = InHWindow;

	Renderer.Create(HWindow);
	FRenderCollector::Initialize(Renderer.GetFD3DDevice().GetDevice());

	MainPanel.Create(HWindow, Renderer, this);

	if (!Scene.empty()) {
		EditorWorld = Scene[0];
	}
	else {
		EditorWorld = UObjectManager::Get().CreateObject<UWorld>();
		Scene.push_back(EditorWorld);
	}
	CurrentWorld = 0;
	Scene[CurrentWorld]->InitWorld();

	EditorGizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	EditorGizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	ViewportClient.SetGizmo(EditorGizmo);

	EditorGizmo->Deactivate();	//	Initially hide the gizmo until an object is selected

	RECT rect;
	GetClientRect(HWindow, &rect);
	WindowWidth = static_cast<float>(rect.right - rect.left);
	WindowHeight = static_cast<float>(rect.bottom - rect.top);

	ViewportClient.Initialize(HWindow);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);
	ViewportClient.SetWorld(Scene[CurrentWorld]);

	EditorCamera = UObjectManager::Get().CreateObject<UCamera>();
	ViewportClient.SetCamera(EditorCamera);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);

	ResetCamera(EditorCamera);
	EditorCamera->ApplyCameraState();
	SyncCameraFromRenderHandler();

	Scene[CurrentWorld]->SetActiveCamera(EditorCamera);
	
	//EditorGizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	//EditorGizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	//ViewportClient.SetGizmo(EditorGizmo);
}

void FEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	WindowWidth = static_cast<float>(Width);
	WindowHeight = static_cast<float>(Height);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);

	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void FEditorEngine::ResetCamera(UCamera* Camera) {
	if (!Camera) return;
	Camera->SetWorldLocation(InitViewPos);
	Camera->LookAt(InitLookAt);
}

void FEditorEngine::ResetViewport() {
	EditorCamera->bPendingKill = true;
	UObjectManager::Get().CollectGarbage();

	EditorCamera = UObjectManager::Get().CreateObject<UCamera>();
	ViewportClient.SetWorld(Scene[CurrentWorld]);
	ViewportClient.SetCamera(EditorCamera);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);
	EditorCamera->ApplyCameraState();
	ResetCamera(EditorCamera);
	SyncCameraFromRenderHandler();
	Scene[CurrentWorld]->SetActiveCamera(EditorCamera);
}

void FEditorEngine::CloseScene() {
	EditorGizmo->bPendingKill = true;

	if (!Scene.empty()) {
		for (UWorld* World : Scene) {
			World->EndPlay();
		}
	}

	UObjectManager::Get().CollectGarbage();
	FRenderCollector::Release();

	//if (EditorGizmo)
	//{
	//	delete EditorGizmo;
	//	EditorGizmo = nullptr;
	//}
}

void FEditorEngine::NewScene() {
	ClearScene();
	UWorld* World = UObjectManager::Get().CreateObject<UWorld>();
	Scene.push_back(World);
	CurrentWorld = 0;
	ResetViewport();
}

void FEditorEngine::Release()
{
	CloseScene();
	MainPanel.Release();
	Renderer.Release();
}

void FEditorEngine::ClearScene() {
	if (!Scene.empty()) {
		for (UWorld* World : Scene) {
			World->EndPlay();
		}
		UObjectManager::Get().CollectGarbage();
		for (auto* W : Scene) {
			W = nullptr;
		}
		Scene.clear();
	}
}

void FEditorEngine::BeginPlay()
{
	if (!Scene.empty() && Scene[CurrentWorld])
	{
		Scene[CurrentWorld]->BeginPlay();
	}
}

void FEditorEngine::BeginFrame(float DeltaTime)
{
	InputSystem::Update();
	ViewportClient.Tick(DeltaTime);
	MainPanel.Update();
	SyncCameraFromRenderHandler();
}

void FEditorEngine::Update(float DeltaTime)
{
}

void FEditorEngine::Render(float DeltaTime)
{
	RenderBus.Clear();
	BuildRenderCommands();

	Renderer.BeginFrame();
	Renderer.Render(RenderBus);
	MainPanel.Render(DeltaTime, ViewportClient.GetViewOutput());
	Renderer.RenderOverlay(RenderBus);	//	UI가 그려진 후 Overlay 그리기
	Renderer.EndFrame();
}

void FEditorEngine::EndFrame()
{
	UObjectManager::Get().CollectGarbage();
}

void FEditorEngine::SyncCameraFromRenderHandler()
{
	if (EditorCamera)
	{
		EditorCamera->ApplyCameraState();
	}
}

void FEditorEngine::UpdateWorld(float DeltaTime)
{
	if (!Scene.empty() && Scene[CurrentWorld])
	{
		Scene[CurrentWorld]->Tick(DeltaTime);
	}
}

void FEditorEngine::BuildRenderCommands()
{
	FRenderCollectorContext Context;
	Context.World = Scene[CurrentWorld];
	Context.Camera = EditorCamera;
	Context.Gizmo = EditorGizmo;
	Context.bGridVisible = RenderHandler.bGridVisible;
	Context.CursorOverlayState = &ViewportClient.GetCursorOverlayState();
	Context.ViewportHeight = WindowHeight;
	Context.ViewportWidth = WindowWidth;
	Context.SelectedComponent = ViewportClient.GetGizmo()->HasTarget() ? (UPrimitiveComponent *)ViewportClient.GetGizmo()->GetTarget() : nullptr;

	FRenderCollector::Collect(Context, RenderBus);
}
