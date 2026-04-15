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

    ViewportLayout.Shutdown();           // 위젯 트리 해제 (소유권: UEditorEngine)
    FSlateApplication::Get().Shutdown(); // RootWindow 해제

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
        if (!Ctx.World || Ctx.bPaused)
            continue;
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
    const EViewportPlayState CurrentState = GetEditorState();

    if (CurrentState == EViewportPlayState::Paused)
    {
        ResumePlaySession();
        return;
    }
    if (CurrentState == EViewportPlayState::Playing) return;

	// 포커스된 뷰포트 클라이언트를 찾고 카메라 상태를 저장한 뒤, 실행 상태를 변경합니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient& FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);
    UWorld* FocusedWorld = GetFocusedWorld();

    if (!FocusedWorld) return;

    FocusedClient.SaveCameraSnapshot();
	// 주의! Editor State는 실제 에디터의 상태가 아닌, 현재 에디터가 포커스한 뷰포트의 상태를 의미합니다.
    SetEditorState(EViewportPlayState::Playing); 

    // PIE 월드 복제하고 세팅한 뒤, RegisterWorld() 헬퍼를 사용해 월드를 WorldList에 등록합니다.
    UWorld* PIEWorld = Cast<UWorld>(FocusedWorld->Duplicate());
    PIEWorld->SetWorldType(EWorldType::PIE);
    FName PIEHandle(("PIE_" + std::to_string(FocusedIdx)).c_str());
    std::string PIEName = "PIE_World_" + std::to_string(FocusedIdx);
    
    RegisterWorld(PIEWorld, EWorldType::PIE, PIEHandle, PIEName);
    ViewportPIEHandles[FocusedIdx] = PIEHandle;

    // 월드를 전환한 뒤 뷰포트에 연결하고, PIE World를 실행합니다.
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
    if (GetEditorState() != EViewportPlayState::Playing)
        return;

    SetEditorState(EViewportPlayState::Paused);

    // PIE 컨텍스트를 일시정지 상태로 표시해 WorldTick에서 제외합니다.
    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
        if (FWorldContext* Ctx = GetWorldContextFromHandle(HandleIt->second))
            Ctx->bPaused = true;
}

void UEditorEngine::ResumePlaySession()
{
    const int32 ResumeIdx = ViewportLayout.GetLastFocusedViewportIndex();
    auto ResumeIt = ViewportPIEHandles.find(ResumeIdx);

    if (ResumeIt != ViewportPIEHandles.end())
    {
        if (FWorldContext* Ctx = GetWorldContextFromHandle(ResumeIt->second))
        {
            Ctx->bPaused = false;
        }
    }

    SetEditorState(EViewportPlayState::Playing);
}

void UEditorEngine::StopPlaySession()
{
    if (GetEditorState() == EViewportPlayState::Editing)
        return;

    const int32 FocusedIdx = ViewportLayout.GetLastFocusedViewportIndex();
    FEditorViewportClient& FocusedClient = ViewportLayout.GetViewportClient(FocusedIdx);

    // 기존 PIE 월드를 해제합니다.
    auto HandleIt = ViewportPIEHandles.find(FocusedIdx);
    if (HandleIt != ViewportPIEHandles.end())
    {
        FName PIEHandle = HandleIt->second;
        ViewportPIEHandles.erase(HandleIt);
        
        UnregisterWorld(PIEHandle);
    }

    // 원본 에디터 월드를 검색합니다.
    FName EditorHandle = GetEditorWorldHandle();
    UWorld* EditorWorld = nullptr;
    
    if (EditorHandle != FName::None)
    {
        SetActiveWorld(EditorHandle);
        if (FWorldContext* Ctx = GetWorldContextFromHandle(EditorHandle))
        {
            EditorWorld = Ctx->World;
        }
    }

    // 원본 에디터 월드로 뷰포트 및 상태를 복구합니다.
    FocusedClient.EndPIE(EditorWorld);
    SetEditorState(EViewportPlayState::Editing);
    FocusedClient.RestoreCameraSnapshot();

    if (ViewportPIEHandles.empty())
    {
        InputSystem::Get().SetCursorVisibility(true);
    }

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

    for (FWorldContext& Ctx : WorldList)
    {
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

// 이미 생성된 월드를 컨텍스트에 등록합니다.
FWorldContext& UEditorEngine::RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handle, const std::string& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.World = InWorld;
    Context.ContextName = Name;
    Context.ContextHandle = Handle;
    
    WorldList.push_back(Context);
    return WorldList.back();
}

// 컨텍스트에서 월드를 찾아 파괴하고 리스트에서 제거합니다.
void UEditorEngine::UnregisterWorld(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            if (it->World)
            {
                it->World->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
                UObjectManager::Get().DestroyObject(it->World);
            }
            WorldList.erase(it);
            return; // 찾아서 지웠으므로 즉시 종료
        }
    }
}

// Editor Context World 핸들을 찾아 반환합니다.
FName UEditorEngine::GetEditorWorldHandle() const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::Editor)
        {
            return Ctx.ContextHandle;
        }
    }
    return FName::None;
}