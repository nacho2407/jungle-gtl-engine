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
	GetFocusedWorld()->SetActiveCamera(GetCamera());

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
	MainPanel.Update();
	WorldTick(DeltaTime);
	Render(DeltaTime);
}

void UEditorEngine::WorldTick(float DeltaTime)
{
	// 포커스된 뷰포트의 카메라를 해당 월드의 ActiveCamera로 설정
	const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
	FEditorViewportClient& FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
	if (UWorld* FocusedWorld = FocusedClient.GetFocusedWorld())
	{
		if (FViewportCamera* Cam = FocusedClient.GetCamera())
		{
			FocusedWorld->SetActiveCamera(Cam);
		}
	}

	// WorldList의 모든 월드에 대해 Tick()을 넣어줍니다.
	// UWorld::Tick에서 EWorldType에 따라 TickEditor / TickGame이 분기됩니다.
	for (FWorldContext& Ctx : WorldList)
	{
		if (!Ctx.World || Ctx.bPaused) continue;
		Ctx.World->Tick(DeltaTime);
	}
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	FViewportRect HostRect = GetViewportLayout().GetHostRect();
	GetRenderer().GetFD3DDevice().EnsureViewportRenderTargets(HostRect.Width, HostRect.Height);
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::StartPlaySession()
{
	// 일시정지 → 재개
	if (GetEditorState() == EViewportPlayState::Paused)
    {
        SetEditorState(EViewportPlayState::Playing);
        const int32 ResumeIdx = ViewportLayout.GetLastFocusedViewportIndex();
        auto ResumeIt = ViewportPIEHandles.find(ResumeIdx);
        if (ResumeIt != ViewportPIEHandles.end())
            if (FWorldContext* Ctx = GetWorldContextFromHandle(ResumeIt->second))
                Ctx->bPaused = false;
        return;
    }

    if (GetEditorState() == EViewportPlayState::Playing) return;

    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient& FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);

    UWorld* EditorWorld = FocusedClient.GetFocusedWorld();
    if (!EditorWorld) return;

    FocusedClient.SaveCameraSnapshot();
    SetEditorState(EViewportPlayState::Playing);

    // 에디터 월드 복제 (Actor 및 Component 깊은 복사) — PostDuplicate 까지 내부에서 처리됩니다.
    UWorld* PIEWorld = Cast<UWorld>(EditorWorld->Duplicate());
    
    PIEWorld->SetWorldType(EWorldType::PIE);

    FName PIEHandle(("PIE_" + std::to_string(FocusedIdx)).c_str());
    FWorldContext PIEContext;
    PIEContext.WorldType     = EWorldType::PIE;
    PIEContext.World         = PIEWorld;
    PIEContext.ContextName   = "PIE_World_" + std::to_string(FocusedIdx);
    PIEContext.ContextHandle = PIEHandle;
    WorldList.push_back(PIEContext);
    ViewportPIEHandles[FocusedIdx] = PIEHandle;

    // 포커스된 뷰포트만 PIE 월드로 전환
    SetActiveWorld(PIEHandle);
    FocusedClient.StartPIE(PIEWorld);
    FocusedClient.SetEndPIECallback([this]() { StopPlaySession(); });

    FocusedClient.LockCursorToViewport();
    InputSystem::Get().SetCursorVisibility(false);

    SelectionManager.ClearSelection();
    PIEWorld->SetActiveCamera(FocusedClient.GetCamera());
    PIEWorld->BeginPlay();
}

void UEditorEngine::PausePlaySession()
{
    if (GetEditorState() != EViewportPlayState::Playing) return;

    SetEditorState(EViewportPlayState::Paused);

    // PIE 컨텍스트를 일시정지 상태로 표시해 WorldTick에서 제외합니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
        if (FWorldContext* Ctx = GetWorldContextFromHandle(HandleIt->second))
            Ctx->bPaused = true;
}

void UEditorEngine::StopPlaySession()
{
    if (GetEditorState() == EViewportPlayState::Editing) return;

    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient& FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);

    // 포커스된 뷰포트의 PIE 핸들 조회
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
    {
        FName PIEHandle = HandleIt->second;
        ViewportPIEHandles.erase(HandleIt);

        for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
        {
            if (it->ContextHandle == PIEHandle)
            {
                it->World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
                UObjectManager::Get().DestroyObject(it->World);
                WorldList.erase(it);
                break;
            }
        }
    }

    // 에디터 월드로 복구
    UWorld* EditorWorld = nullptr;
    FName EditorHandle = FName::None;
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::Editor)
        {
            EditorWorld = Ctx.World;
            EditorHandle = Ctx.ContextHandle;
            break;
        }
    }

    FocusedClient.EndPIE(EditorWorld);
    SetEditorState(EViewportPlayState::Editing);
    FocusedClient.RestoreCameraSnapshot();

    if (EditorHandle != FName::None)
        SetActiveWorld(EditorHandle);

    if (ViewportPIEHandles.empty())
        InputSystem::Get().SetCursorVisibility(true);

    SelectionManager.ClearSelection();
}

void UEditorEngine::ResetViewport()
{
	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		FEditorViewportClient& ViewportClient = ViewportLayout.GetViewportClient(i);
		ViewportClient.CreateCamera();
		ViewportClient.SetWorld(GetFocusedWorld());
		ViewportClient.ApplyCameraMode();
	}
	
	// 디폴트로 0번 뷰포트의 카메라를 월드 활성 카메라로 재등록
	GetFocusedWorld()->SetActiveCamera(ViewportLayout.GetIndexedViewportClientCamera(0));
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
	UWorld* World = (TargetWorld != nullptr) ? TargetWorld : GetFocusedWorld();
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
