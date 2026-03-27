#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Core/Stats.h"

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

	// LevelViewportClient 생성 (현재는 1개, 추후 4분할 시 확장)
	auto* LevelVC = new FLevelEditorViewportClient();
	LevelVC->SetSettings(&FEditorSettings::Get());
	LevelVC->Initialize(Window);
	LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
	LevelVC->SetWorld(GetWorld());
	LevelVC->SetGizmo(SelectionManager.GetGizmo());
	LevelVC->SetSelectionManager(&SelectionManager);

	LevelVC->CreateCamera();
	LevelVC->ResetCamera();
	GetWorld()->SetActiveCamera(LevelVC->GetCamera());

	// 배열에 등록
	AllViewportClients.push_back(LevelVC);
	LevelViewportClients.push_back(LevelVC);

	// Editor render pipeline
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// ViewportClient 해제
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		delete VC;
	}
	AllViewportClients.clear();
	LevelViewportClients.clear();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
	}
}

void UEditorEngine::Tick(float DeltaTime)
{
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->Tick(DeltaTime);
	}
	MainPanel.Update();
	UEngine::Tick(DeltaTime);
}

UCameraComponent* UEditorEngine::GetCamera() const
{
	if (!LevelViewportClients.empty())
	{
		return LevelViewportClients[0]->GetCamera();
	}
	return nullptr;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::ResetViewport()
{
	if (LevelViewportClients.empty()) return;

	FLevelEditorViewportClient* LevelVC = LevelViewportClients[0];
	LevelVC->CreateCamera();
	LevelVC->SetWorld(GetWorld());
	LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
	LevelVC->ResetCamera();
	GetWorld()->SetActiveCamera(LevelVC->GetCamera());
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

	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->DestroyCamera();
		VC->SetWorld(nullptr);
	}
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

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;

	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->DestroyCamera();
		VC->SetWorld(nullptr);
	}
}
