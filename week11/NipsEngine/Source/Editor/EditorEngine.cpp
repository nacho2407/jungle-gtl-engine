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
#include "Settings/EditorSettings.h"

DEFINE_CLASS(UEditorEngine, UEngine)
REGISTER_FACTORY(UEditorEngine)

//  Init
void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);
	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

	MainPanel.Create(Window, Renderer, this);
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Editor, FName("Default"));
	}
	SetActiveWorld(WorldList[0].ContextHandle);

	// Selection & Gizmo
	SelectionManager.Init();
	ViewportLayout.Init(InWindow, GetWorld(), &SelectionManager);
	GetWorld()->SetActiveCamera(GetCamera());

	// Slate 초기화 및 Viewport Layout 추가
	FSlateApplication::Get().Initialize();
	ViewportLayout.BuildViewportLayout(static_cast<int32>(Window->GetWidth()), static_cast<int32>(Window->GetHeight()));

	// Editor용 렌더 파이프라인 세팅
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
}

void UEditorEngine::Shutdown()
{
	// 스플리터 비율을 Settings 에 기록 후 저장
	if (SSplitterV* SV = ViewportLayout.GetRootSplitterV())
		FEditorSettings::Get().SplitterVRatio = SV->GetSplitRatio();
	if (SSplitterH* SH = ViewportLayout.GetTopSplitterH())
		FEditorSettings::Get().SplitterHRatio = SH->GetSplitRatio();

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
	// hover/focus 상태를 먼저 갱신한 뒤 ActiveCamera 를 업데이트합니다.
	// 이렇게 해야 같은 프레임의 첫 클릭에서도 올바른 카메라로 피킹할 수 있습니다.
	// ViewportLayout.UpdateHoverStates();
	ViewportLayout.Tick(DeltaTime);

	if (UWorld* World = GetWorld())
	{
		// 활성화된 카메라 갱신
		const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
		if (FViewportCamera* FocusedCam = ViewportLayout.GetIndexedViewportClientCamera(FocusedIdx))
			World->SetActiveCamera(FocusedCam);
	}

	MainPanel.Update();
	UEngine::Tick(DeltaTime);
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	FViewportRect HostRect = GetViewportLayout().GetHostRect();
	GetRenderer().GetFD3DDevice().EnsureViewportRenderTargets(HostRect.Width, HostRect.Height);
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
		Ctx.World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
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
		Ctx.World->EndPlay(EEndPlayReason::Type::LevelTransition);
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