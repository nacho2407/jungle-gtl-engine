#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"
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

	// FViewport 생성 — 오프스크린 RT
	auto* VP = new FViewport();
	VP->Initialize(Renderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));
	VP->SetClient(LevelVC);
	LevelVC->SetViewport(VP);

	// 카메라 생성 — 각 VC가 자체 카메라 소유
	LevelVC->CreateCamera();
	LevelVC->ResetCamera();

	// 배열에 등록
	AllViewportClients.push_back(LevelVC);
	LevelViewportClients.push_back(LevelVC);

	// 첫 번째 뷰포트를 활성으로 설정
	SetActiveViewport(LevelVC);

	// ViewportWidget에 ViewportClient 연결
	MainPanel.GetViewportWidget(0).SetViewportClient(LevelVC);

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

	// ViewportClient + FViewport 해제
	ActiveViewportClient = nullptr;
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		if (FViewport* VP = VC->GetViewport())
		{
			VP->Release();
			delete VP;
		}
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
	// 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
	// FViewport RT는 ViewportWidget에서 지연 리사이즈로 처리됨
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
	if (ActiveViewportClient)
	{
		return ActiveViewportClient->GetCamera();
	}
	return nullptr;
}

void UEditorEngine::SetActiveViewport(FLevelEditorViewportClient* InClient)
{
	// 이전 활성 뷰포트 비활성화
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(false);
	}

	ActiveViewportClient = InClient;

	// 새 활성 뷰포트 활성화 + World의 ActiveCamera 갱신
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(true);

		UWorld* World = GetWorld();
		if (World && ActiveViewportClient->GetCamera())
		{
			World->SetActiveCamera(ActiveViewportClient->GetCamera());
		}
	}
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::ToggleViewportSplit()
{
	bIsSplitViewport = !bIsSplitViewport;

	if (bIsSplitViewport)
	{
		// 4분할: 추가 3개 LevelViewportClient + FViewport + Camera 생성
		for (int32 i = 1; i < FEditorMainPanel::MaxViewports; ++i)
		{
			auto* LevelVC = new FLevelEditorViewportClient();
			LevelVC->SetSettings(&FEditorSettings::Get());
			LevelVC->Initialize(Window);
			LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
			LevelVC->SetWorld(GetWorld());
			LevelVC->SetGizmo(SelectionManager.GetGizmo());
			LevelVC->SetSelectionManager(&SelectionManager);

			auto* VP = new FViewport();
			VP->Initialize(Renderer.GetFD3DDevice().GetDevice(),
				static_cast<uint32>(Window->GetWidth()),
				static_cast<uint32>(Window->GetHeight()));
			VP->SetClient(LevelVC);
			LevelVC->SetViewport(VP);

			LevelVC->CreateCamera();
			LevelVC->ResetCamera();

			AllViewportClients.push_back(LevelVC);
			LevelViewportClients.push_back(LevelVC);

			MainPanel.GetViewportWidget(i).SetViewportClient(LevelVC);
		}

		MainPanel.SetActiveViewportCount(FEditorMainPanel::MaxViewports);
	}
	else
	{
		// 1뷰포트: 추가 3개 제거 (index 1~3)
		while (LevelViewportClients.size() > 1)
		{
			FLevelEditorViewportClient* VC = LevelViewportClients.back();
			LevelViewportClients.pop_back();

			// AllViewportClients에서도 제거
			for (auto It = AllViewportClients.begin(); It != AllViewportClients.end(); ++It)
			{
				if (*It == VC)
				{
					AllViewportClients.erase(It);
					break;
				}
			}

			// 활성 뷰포트가 제거 대상이면 0번으로 전환
			if (ActiveViewportClient == VC)
			{
				SetActiveViewport(LevelViewportClients[0]);
			}

			if (FViewport* VP = VC->GetViewport())
			{
				VP->Release();
				delete VP;
			}
			VC->DestroyCamera();
			delete VC;
		}

		// Widget 연결 해제
		for (int32 i = 1; i < FEditorMainPanel::MaxViewports; ++i)
		{
			MainPanel.GetViewportWidget(i).SetViewportClient(nullptr);
		}

		MainPanel.SetActiveViewportCount(1);
	}
}

void UEditorEngine::ResetViewport()
{
	// 모든 뷰포트 카메라 재생성
	for (FLevelEditorViewportClient* VC : LevelViewportClients)
	{
		VC->CreateCamera();
		VC->SetWorld(GetWorld());
		VC->ResetCamera();
	}

	// 활성 뷰포트의 카메라를 World에 설정
	if (ActiveViewportClient && GetWorld())
	{
		GetWorld()->SetActiveCamera(ActiveViewportClient->GetCamera());
	}
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
