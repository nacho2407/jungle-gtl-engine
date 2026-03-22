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
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Editor, FName("Default"));
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// Gizmo
	EditorGizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	EditorGizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	EditorGizmo->Deactivate();

	// ViewportClient
	ViewportClient.SetSettings(&FEditorSettings::Get());
	ViewportClient.Initialize(Window);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
	ViewportClient.SetWorld(GetWorld());
	ViewportClient.SetGizmo(EditorGizmo);

	// Camera
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();
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

void UEditorEngine::ResetViewport()
{
	ViewportClient.CreateCamera();
	ViewportClient.SetWorld(GetWorld());
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
	ViewportClient.ResetCamera();
}

void UEditorEngine::CloseScene()
{
	for (FWorldContext& Ctx : WorldList) {
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}
	WorldList.clear();
	ActiveWorldHandle = FName::None;

	ViewportClient.DestroyCamera();

	UObjectManager::Get().DestroyObject(EditorGizmo);
	EditorGizmo = nullptr;

	ViewportClient.SetGizmo(nullptr);
	ViewportClient.SetWorld(nullptr);
}

void UEditorEngine::NewScene()
{
	ClearScene();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
	SetActiveWorld(Ctx.ContextHandle);

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
	for (FWorldContext& Ctx : WorldList) {
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}
	WorldList.clear();
	ActiveWorldHandle = FName::None;

	ViewportClient.DestroyCamera();
	ViewportClient.SetWorld(nullptr);
}

void UEditorEngine::BuildRenderCommands()
{
	FRenderCollectorContext Context;
	Context.World = GetWorld();
	Context.Camera = ViewportClient.GetCamera();
	Context.Gizmo = EditorGizmo;
	Context.ViewMode = FEditorSettings::Get().ViewMode;
	Context.ShowFlags = FEditorSettings::Get().ShowFlags;
	Context.CursorOverlayState = &ViewportClient.GetCursorOverlayState();
	Context.ViewportHeight = Window->GetHeight();
	Context.ViewportWidth = Window->GetWidth();
	Context.SelectedComponent = ViewportClient.GetGizmo()->HasTarget() ? (UPrimitiveComponent*)ViewportClient.GetGizmo()->GetTarget() : nullptr;

	FRenderCollector::Collect(Context, RenderBus);
}
