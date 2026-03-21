#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderCollectorContext.h"

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);

	// 에디터 전용 초기화
	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

	MainPanel.Create(Window, Renderer, this);

	// World
	if (!Scene.empty()) {
		// already has a world
	}
	else {
		UWorld* World = UObjectManager::Get().CreateObject<UWorld>();
		Scene.push_back(World);
	}
	CurrentWorld = 0;
	Scene[CurrentWorld]->InitWorld();

	// Gizmo
	EditorGizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	EditorGizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	EditorGizmo->Deactivate();

	// ViewportClient
	ViewportClient.SetSettings(&FEditorSettings::Get());
	ViewportClient.Initialize(Window);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
	ViewportClient.SetWorld(Scene[CurrentWorld]);
	ViewportClient.SetGizmo(EditorGizmo);

	// Camera
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
	ViewportClient.SetCamera(Camera);
	ResetCamera(Camera);
	Scene[CurrentWorld]->SetActiveCamera(Camera);
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	MainPanel.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
}

void UEditorEngine::Tick(float DeltaTime)
{
	ViewportClient.Tick(DeltaTime);
	MainPanel.Update();
	UEngine::Tick(DeltaTime);
}

void UEditorEngine::Render(float DeltaTime)
{
	RenderBus.Clear();
	BuildRenderCommands();

	ERasterizerState ViewModeRasterizer = ERasterizerState::SolidBackCull;
	if (FEditorSettings::Get().ViewMode == EViewMode::Wireframe)
	{
		ViewModeRasterizer = ERasterizerState::WireFrame;
	}

	Renderer.BeginFrame();
	Renderer.Render(RenderBus, ViewModeRasterizer);
	MainPanel.Render(DeltaTime, ViewportClient.GetViewOutput());
	Renderer.RenderOverlay(RenderBus);
	Renderer.EndFrame();
}

void UEditorEngine::ResetCamera(UCameraComponent* InCamera)
{
	if (!InCamera) return;
	InCamera->SetWorldLocation(FEditorSettings::Get().InitViewPos);
	InCamera->LookAt(FEditorSettings::Get().InitLookAt);
}

void UEditorEngine::ResetViewport()
{
	UObjectManager::Get().DestroyObject(Camera);
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
	ViewportClient.SetWorld(Scene[CurrentWorld]);
	ViewportClient.SetCamera(Camera);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
	ResetCamera(Camera);
	Scene[CurrentWorld]->SetActiveCamera(Camera);
}

void UEditorEngine::CloseScene()
{
	if (!Scene.empty()) {
		for (UWorld* World : Scene) {
			World->EndPlay();
			UObjectManager::Get().DestroyObject(World);
		}
		Scene.clear();
	}

	UObjectManager::Get().DestroyObject(Camera);
	Camera = nullptr;

	UObjectManager::Get().DestroyObject(EditorGizmo);
	EditorGizmo = nullptr;

	ViewportClient.SetCamera(nullptr);
	ViewportClient.SetGizmo(nullptr);
	ViewportClient.SetWorld(nullptr);
}

void UEditorEngine::NewScene()
{
	ClearScene();
	UWorld* World = UObjectManager::Get().CreateObject<UWorld>();
	Scene.push_back(World);
	CurrentWorld = 0;

	// EditorGizmo가 CloseScene()으로 파괴된 경우 재생성
	if (!EditorGizmo)
	{
		EditorGizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
		EditorGizmo->Deactivate();
		ViewportClient.SetGizmo(EditorGizmo);
	}

	ResetViewport();
}

void UEditorEngine::ClearScene()
{
	if (!Scene.empty()) {
		for (UWorld* World : Scene) {
			World->EndPlay();
			UObjectManager::Get().DestroyObject(World);
		}
		Scene.clear();
	}

	UObjectManager::Get().DestroyObject(Camera);
	Camera = nullptr;

	ViewportClient.SetCamera(nullptr);
	ViewportClient.SetWorld(nullptr);
}

void UEditorEngine::BuildRenderCommands()
{
	FRenderCollectorContext Context;
	Context.World = Scene[CurrentWorld];
	Context.Camera = Camera;
	Context.Gizmo = EditorGizmo;
	Context.ViewMode = FEditorSettings::Get().ViewMode;
	Context.ShowFlags = FEditorSettings::Get().ShowFlags;
	Context.CursorOverlayState = &ViewportClient.GetCursorOverlayState();
	Context.ViewportHeight = Window->GetHeight();
	Context.ViewportWidth = Window->GetWidth();
	Context.SelectedComponent = ViewportClient.GetGizmo()->HasTarget() ? (UPrimitiveComponent*)ViewportClient.GetGizmo()->GetTarget() : nullptr;

	FRenderCollector::Collect(Context, RenderBus);
}
