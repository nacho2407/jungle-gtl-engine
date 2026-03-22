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

	// Selection & Gizmo
	SelectionManager.Init();

	// ViewportClient
	ViewportClient.SetSettings(&FEditorSettings::Get());
	ViewportClient.Initialize(Window);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
	ViewportClient.SetWorld(GetWorld());
	ViewportClient.SetGizmo(SelectionManager.GetGizmo());
	ViewportClient.SetSelectionManager(&SelectionManager);

	// Camera
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	SelectionManager.Shutdown();
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
	MainPanel.Render(DeltaTime);
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
	SelectionManager.ClearSelection();

	for (FWorldContext& Ctx : WorldList) {
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}
	WorldList.clear();
	ActiveWorldHandle = FName::None;

	ViewportClient.DestroyCamera();
	ViewportClient.SetWorld(nullptr);
}

void UEditorEngine::NewScene()
{
	ClearScene();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
	SetActiveWorld(Ctx.ContextHandle);

	ResetViewport();
}

void UEditorEngine::ClearScene()
{
	SelectionManager.ClearSelection();

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
	Context.Gizmo = SelectionManager.GetGizmo();
	Context.ViewMode = FEditorSettings::Get().ViewMode;
	Context.ShowFlags = FEditorSettings::Get().ShowFlags;
	Context.CursorOverlayState = &ViewportClient.GetCursorOverlayState();
	Context.ViewportHeight = Window->GetHeight();
	Context.ViewportWidth = Window->GetWidth();
	Context.SelectedComponent = nullptr;
	AActor* Primary = SelectionManager.GetPrimarySelection();
	if (Primary && Primary->GetRootComponent() && Primary->GetRootComponent()->IsA<UPrimitiveComponent>())
	{
		Context.SelectedComponent = static_cast<UPrimitiveComponent*>(Primary->GetRootComponent());
	}

	RenderCollector.Collect(Context, RenderBus);
}
