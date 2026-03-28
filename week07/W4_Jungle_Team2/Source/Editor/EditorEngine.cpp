#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Core/Logging/Stats.h"

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

//  뷰포트 타입 테이블  [인덱스 → EEditorViewportType]
static constexpr EEditorViewportType kViewportTypes[UEditorEngine::MaxViewports] =
{
	EVT_Perspective,   // 0 : 좌상단 (원근)
	EVT_OrthoTop,      // 1 : 우상단 (탑 뷰)
	EVT_OrthoFront,    // 2 : 좌하단 (프론트 뷰)
	EVT_OrthoRight,    // 3 : 우하단 (라이트 뷰)
};

//  영역 계산 헬퍼
void UEditorEngine::UpdateViewportRects(uint32 Width, uint32 Height)
{
	const int32 W = static_cast<int32>(Width);
	const int32 H = static_cast<int32>(Height);

	// Step 5(Splitter) 구현 전: 50:50 고정 분할
	const int32 HalfW = W / 2;
	const int32 HalfH = H / 2;

	ViewportStates[0].Rect = { 0,     0,     HalfW,      HalfH      };  // 좌상단
	ViewportStates[1].Rect = { HalfW, 0,     W - HalfW,  HalfH      };  // 우상단
	ViewportStates[2].Rect = { 0,     HalfH, HalfW,      H - HalfH  };  // 좌하단
	ViewportStates[3].Rect = { HalfW, HalfH, W - HalfW,  H - HalfH  };  // 우하단

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		SceneViewports[i].SetRect(ViewportStates[i].Rect);

		AllViewportClients[i].SetViewportSize(
			static_cast<float>(ViewportStates[i].Rect.Width),
			static_cast<float>(ViewportStates[i].Rect.Height));
	}
}

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

	// 초기 뷰포트 영역 설정
	UpdateViewportRects(static_cast<uint32>(Window->GetWidth()), static_cast<uint32>(Window->GetHeight()));

	// 4개 뷰포트 클라이언트 초기화
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		FEditorViewportClient& VC = AllViewportClients[i];

		VC.SetSettings(&FEditorSettings::Get());
		VC.Initialize(Window);
		VC.SetWorld(GetWorld());
		VC.SetGizmo(SelectionManager.GetGizmo());
		VC.SetSelectionManager(&SelectionManager);

		// 상호 참조 연결
		SceneViewports[i].SetClient(&VC);
		VC.SetViewport(&SceneViewports[i]);
		VC.SetState(&ViewportStates[i]);

		// 뷰포트 타입 설정 후 카메라 생성
		VC.SetViewportType(kViewportTypes[i]);
		VC.CreateCamera();
		VC.ApplyCameraMode();
	}

	// 퍼스펙티브 카메라(0번)를 월드 활성 카메라로 등록
	GetWorld()->SetActiveCamera(AllViewportClients[0].GetCamera());

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

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	UpdateViewportRects(Width, Height);
}

void UEditorEngine::Tick(float DeltaTime)
{
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AllViewportClients[i].Tick(DeltaTime);
	}

	MainPanel.Update();
	UEngine::Tick(DeltaTime);
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::ResetViewport()
{
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AllViewportClients[i].CreateCamera();
		AllViewportClients[i].SetWorld(GetWorld());
		AllViewportClients[i].ApplyCameraMode();
	}

	// 퍼스펙티브 카메라를 월드 활성 카메라로 재등록
	GetWorld()->SetActiveCamera(AllViewportClients[0].GetCamera());
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

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AllViewportClients[i].DestroyCamera();
		AllViewportClients[i].SetWorld(nullptr);
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

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AllViewportClients[i].DestroyCamera();
		AllViewportClients[i].SetWorld(nullptr);
	}
}
