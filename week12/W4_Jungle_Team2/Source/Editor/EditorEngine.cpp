#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Slate/SlateApplication.h"
#include "Engine/Core/InputSystem.h"
#include "Runtime/ViewportRect.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Core/Logging/Stats.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

//  Init
void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);
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

	//  뷰포트 초기화
	ViewportLayout.Init(InWindow, GetWorld(), &SelectionManager);

	// 퍼스펙티브 카메라(0번)를 월드 활성 카메라로 등록
	GetWorld()->SetActiveCamera(GetCamera());

	// Slate 초기화 + 위젯 트리 구성 (SSplitterV → 2×SSplitterH → 4×SViewport)
	FSlateApplication::Get().Initialize();
	ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));

	// Editor render pipeline
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());

	ViewportLayout.Shutdown();          // 위젯 트리 해제 (소유권: UEditorEngine)
	FSlateApplication::Get().Shutdown();  // RootWindow 해제

	CloseScene();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	ViewportLayout.OnWindowResized(Width, Height);
}

void UEditorEngine::Tick(float DeltaTime)
{
	ViewportLayout.Tick(DeltaTime);
	MainPanel.Update();
	UEngine::Tick(DeltaTime);
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::ResetViewport()
{
	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		FEditorViewportClient& ViewportClient = ViewportLayout.GetViewportClient(i);
		ViewportClient.CreateCamera();
		ViewportClient.SetWorld(GetWorld());
		ViewportClient.ApplyCameraMode();
	}
	
	// 디폴트로 0번 뷰포트의 카메라를 월드 활성 카메라로 재등록
	GetWorld()->SetActiveCamera(ViewportLayout.GetIndexedViewportClientCamera(0));
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

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		FEditorViewportClient& ViewportClient = ViewportLayout.GetViewportClient(i);
		ViewportClient.DestroyCamera();
		ViewportClient.SetWorld(nullptr);
	}
}

void UEditorEngine::NewScene()
{
	ClearScene();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
	SetActiveWorld(Ctx.ContextHandle);

	ResetViewport();
}

FViewportCamera* UEditorEngine::GetCamera()
{
	return ViewportLayout.GetIndexedViewportClientCamera(0);
}

const FViewportCamera* UEditorEngine::GetCamera() const
{
	return ViewportLayout.GetIndexedViewportClientCamera(0);
}

void UEditorEngine::ClearScene()
{
	SelectionManager.ClearSelection();

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		FEditorViewportClient& ViewportClient = ViewportLayout.GetViewportClient(i);
		ViewportClient.DestroyCamera();
		ViewportClient.SetWorld(nullptr);
	}
}