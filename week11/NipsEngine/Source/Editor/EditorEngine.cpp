#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Slate/SlateApplication.h"
#include "Engine/Input/InputSystem.h"
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
#include <algorithm>

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
	ApplySpatialIndexMaintenanceSettings();

	// Selection & Gizmo
	SelectionManager.Init();
	ViewportLayout.Init(InWindow, GetWorld(), &SelectionManager, this);
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
    InputSystem::Get().Tick();
	ViewportLayout.Tick(DeltaTime);

	if (UWorld* World = GetWorld())
	{
		// 활성화된 카메라 갱신
		const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
		if (FViewportCamera* FocusedCam = ViewportLayout.GetIndexedViewportClientCamera(FocusedIdx))
			World->SetActiveCamera(FocusedCam);
	}

	MainPanel.Update();
	if (GetEditorState() == EEditorState::Play)
	{
		WorldTick(DeltaTime);
	}
	Render(DeltaTime);
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	FViewportRect HostRect = GetViewportLayout().GetHostRect();
	GetRenderer().GetFD3DDevice().EnsureViewportRenderTargets(HostRect.Width, HostRect.Height);
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::StartPlaySession()
{
	// 일시정지 상태라면, 새로 복제하지 않고 Early Return
	if (EditorState == EEditorState::Pause)
    {
        SetEditorState(EEditorState::Play);
        return;
    }

	// 이미 플레이 중이라면 무시
    if (EditorState == EEditorState::Play) return;

    UWorld* EditorWorld = GetWorld();
    if (!EditorWorld) return;

    SetEditorState(EEditorState::Play);

    // 에디터 월드 복제 (Actor 및 Component 깊은 복사)
    UWorld* PIEWorld = EditorWorld->Duplicate();
    PIEWorld->DuplicateSubObjects();
    
    // PIE 용도로 타입 변경 후 WorldList에 PIE Context 등록
    PIEWorld->SetWorldType(EWorldType::PIE);
    FWorldContext PIEContext;
    PIEContext.WorldType = EWorldType::PIE;
    PIEContext.World = PIEWorld;
    PIEContext.ContextName = "PIE_World";
    PIEContext.ContextHandle = FName("PIE_World_Handle");
    WorldList.push_back(PIEContext);

    // 활성 월드를 전환하고, 뷰포트가 바라보는 월드를 PIE 월드로 교체
    SetActiveWorld(PIEContext.ContextHandle);
    for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
    {
        ViewportLayout.GetViewportClient(i).StartPIE(PIEWorld);
        ViewportLayout.GetViewportClient(i).SetEndPIECallback([this]() { StopPlaySession(); });
    }

	ViewportLayout.SetLastFocusedViewportIndex(0);
    ViewportLayout.GetViewportClient(0).LockCursorToViewport();
    InputSystem::Get().SetCursorVisibility(false);

    PIEWorld->SetActiveCamera(GetCamera());

    // 이전 에디터에서 선택했던 액터 포인터 해제 (복제본과 섞이지 않도록)
    SelectionManager.ClearSelection();

    PIEWorld->BeginPlay();
}

void UEditorEngine::PausePlaySession()
{
	if (EditorState != EEditorState::Play) return;
    
    SetEditorState(EEditorState::Pause);
}

void UEditorEngine::StopPlaySession()
{
	if (EditorState == EEditorState::Edit) return;

    SetEditorState(EEditorState::Edit);

	InputSystem::Get().SetCursorVisibility(true);

    UWorld* PIEWorld = nullptr;
    FName EditorContextHandle = FName::None;

    // WorldList를 순회하며 PIE 월드를 찾아 파괴하고, Editor 월드 핸들을 찾습니다.
    for (auto it = WorldList.begin(); it != WorldList.end(); )
    {
        if (it->WorldType == EWorldType::PIE)
        {
            PIEWorld = it->World;
            PIEWorld->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
            UObjectManager::Get().DestroyObject(PIEWorld);
            it = WorldList.erase(it);
        }
        else
        {
            if (it->WorldType == EWorldType::Editor)
            {
                EditorContextHandle = it->ContextHandle;
            }
            ++it;
        }
    }

    // 기존 에디터 월드로 원상 복구
    if (EditorContextHandle != FName::None)
    {
        SetActiveWorld(EditorContextHandle);
        UWorld* EditorWorld = GetWorld();

        for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
        {
            ViewportLayout.GetViewportClient(i).EndPIE(EditorWorld);
        }
        
        if (EditorWorld)
        {
            EditorWorld->SetActiveCamera(GetCamera());
        }
    }

    // 삭제된 PIE 액터의 쓰레기 포인터가 남아있을 수 있으므로 선택 상태 강제 초기화
    SelectionManager.ClearSelection();
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
	ApplySpatialIndexMaintenanceSettings(Ctx.World);

	ResetViewport();
}

void UEditorEngine::ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld)
{
	UWorld* World = (TargetWorld != nullptr) ? TargetWorld : GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FEditorSettings& Settings = GetSettings();
	FWorldSpatialIndex::FMaintenancePolicy& Policy = World->GetSpatialIndex().GetMaintenancePolicy();

	Policy.BatchRefitMinDirtyCount = std::max<int32>(1, Settings.SpatialBatchRefitMinDirtyCount);
	Policy.BatchRefitDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialBatchRefitDirtyPercentThreshold, 1, 100);
	Policy.RotationStructuralChangeThreshold = std::max<int32>(1, Settings.SpatialRotationStructuralChangeThreshold);
	Policy.RotationDirtyCountThreshold = std::max<int32>(1, Settings.SpatialRotationDirtyCountThreshold);
	Policy.RotationDirtyPercentThreshold = std::clamp<int32>(Settings.SpatialRotationDirtyPercentThreshold, 1, 100);
}

FViewportCamera* UEditorEngine::GetCamera()
{
	return ViewportLayout.GetIndexedViewportClientCamera(0);
}

const FViewportCamera* UEditorEngine::GetCamera() const
{
	return ViewportLayout.GetIndexedViewportClientCamera(0);
}

FEditorRenderPipeline* UEditorEngine::GetEditorRenderPipeline() const
{
	return static_cast<FEditorRenderPipeline*>(GetRenderPipeline());
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
