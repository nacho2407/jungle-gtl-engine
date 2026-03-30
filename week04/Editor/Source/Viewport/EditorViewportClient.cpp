#include "Viewport/EditorViewportClient.h"

#include <cstdio>

#include "ApplicationCore/Input/InputRouter.h"
#include "Editor/EditorContext.h"
#include "Engine/Scene.h"
#include "Engine/Game/Actor.h"

#include "imgui.h"

void FEditorViewportClient::Create()
{
    ActiveController = &OwnedSelectionController;

    InputRouter = new Engine::ApplicationCore::FInputRouter();
    InputRouter->AddContext(&ViewportInputContext);
    InputRouter->AddContext(&SelectionInputContext);
    InputRouter->AddContext(&GizmoInputContext);
    GizmoController.SetViewportClient(this);
    GizmoController.SetViewportSelectionController(ActiveController);

    NavigationController.SetCamera(&ViewportCamera);
    NavigationController.SetSelectionController(ActiveController);
    ActiveController->SetCamera(&ViewportCamera);
    SelectionInputContext.SetSelectionController(ActiveController);
    GizmoController.SetCamera(&ViewportCamera);
    GizmoInputContext.SetNavigationController(&NavigationController);

    ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
    ViewportCamera.SetFOV(3.141592f * 0.5f);
    ViewportCamera.SetNearPlane(0.1f);
    ViewportCamera.SetFarPlane(2000.0f);
    ViewportCamera.SetLocation(FVector(-20.0f, 1.0f, 10.0f));
    ViewportCamera.SetRotation(FRotator(0.0f, 0.0f, 0.0f));
}

void FEditorViewportClient::Release()
{
    if (InputRouter)
    {
        delete InputRouter;
        InputRouter = nullptr;
    }
}

void FEditorViewportClient::Initialize(FScene* Scene, uint32 ViewportWidth, uint32 ViewportHeight)
{
    SetScene(Scene);

    ViewportCamera.OnResize(ViewportWidth, ViewportHeight);

    ActiveController->SetActors(Scene->GetActors());
    ActiveController->SetCamera(&ViewportCamera);
    ActiveController->SetViewportSize(ViewportWidth, ViewportHeight);
}

void FEditorViewportClient::Tick(float DeltaTime, const Engine::ApplicationCore::FInputState& State)
{
    ViewportInputContext.SetDeltaTime(DeltaTime);

    if (InputRouter)
    {
        InputRouter->Tick(State);
    }

    ActiveController->SyncSelectionFromContext();
    NavigationController.Tick(DeltaTime);
}

void FEditorViewportClient::Draw() {}

void FEditorViewportClient::HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                             const Engine::ApplicationCore::FInputState& State)
{
    // Stamp this panel's camera onto the (possibly shared) selection controller so that
    // ray-picking and screen-space rect selection use the correct frustum.
    if (ActiveController != nullptr)
    {
        ActiveController->SetCamera(&ViewportCamera);
        ActiveController->SetViewportSize(ViewportCamera.GetWidth(), ViewportCamera.GetHeight());
    }

    if (InputRouter)
    {
        InputRouter->RouteEvent(Event, State);
    }
}

void FEditorViewportClient::BuildRenderData(FEditorRenderData& OutRenderData, EEditorShowFlags InShowFlags)
{
    OutRenderData.bShowGrid = IsFlagSet(InShowFlags, EEditorShowFlags::SF_Grid);
    OutRenderData.bShowWorldAxes = IsFlagSet(InShowFlags, EEditorShowFlags::SF_WorldAxes);
    OutRenderData.bShowSelectionOutline =
        IsFlagSet(InShowFlags, EEditorShowFlags::SF_SelectionOutline);


    if (!ActiveController->GetSelectedActors().empty())
    {
        OutRenderData.Gizmo.GizmoType = GizmoController.GetGizmoType();
        OutRenderData.Gizmo.Highlight = GizmoController.GetGizmoHighlight();
        GizmoController.SetSelectedActor(ActiveController->GetSelectedActors().back());
        if (GizmoController.bIsWorldMode && GizmoController.GetGizmoType() != EGizmoType::Scaling)
        {
            FVector RelativeLocation{
                GizmoController.GetSelectedActor()->GetRootComponent()->GetRelativeLocation()};
            OutRenderData.Gizmo.Frame = FMatrix::MakeTranslation(RelativeLocation);
        }
        else
        {
            OutRenderData.Gizmo.Frame =
                GizmoController.GetSelectedActor()->GetRootComponent()->GetRelativeMatrixNoScale();
        }
        if (ViewportCamera.GetProjectionType() == EViewportProjectionType::Orthographic)
        {
            GizmoController.GizmoScale = ViewportCamera.GetOrthoHeight() / 10.f;
        }
        else
        {
            GizmoController.GizmoScale =
                (ViewportCamera.GetLocation() -
                 GizmoController.GetSelectedActor()->GetRootComponent()->GetRelativeLocation())
                    .Size() / 10.f;
        }
        //  Size 여기서 조정
        OutRenderData.Gizmo.Scale = GizmoController.GizmoScale;
        OutRenderData.bShowGizmo = IsFlagSet(InShowFlags, EEditorShowFlags::SF_Gizmo);
        GizmoController.bIsDrawed = OutRenderData.bShowGizmo;
    }
    else
    {
        OutRenderData.Gizmo.GizmoType = EGizmoType::None;
        OutRenderData.Gizmo.Highlight = EGizmoHighlight::None;
        OutRenderData.Gizmo.Frame = FMatrix::Identity;
        OutRenderData.Gizmo.Scale = 1.0f;
        OutRenderData.bShowGizmo = false;
        GizmoController.SetSelectedActor(nullptr);
        GizmoController.bIsDrawed = false;
    }
}

void FEditorViewportClient::OnResize(uint32 Width, uint32 Height)
{
    ViewportCamera.OnResize(Width, Height);
    ActiveController->SetViewportSize(Width, Height);
}

FString FEditorViewportClient::GetViewOrientationString(EViewportViewOrientation InOrientation) const 
{ 
    using enum EViewportViewOrientation;
    switch (InOrientation)
    {
    case Free:
        return "Free";
    case Top:
        return "Top";
    case Bottom:
        return "Bottom";
    case Left:
        return "Left";
    case Right:
        return "Right";
    case Front:
        return "Front";
    case Back:
        return "Back";
    default:
        return "Unknown";
    }
}

void FEditorViewportClient::SetViewOrientation(EViewportViewOrientation InOrientation) 
{
    if (InOrientation == ViewOrientation)
    {
        return;
    }

    using enum EViewportViewOrientation;
    const bool bWasFree = (ViewOrientation == Free);
    ViewOrientation = InOrientation;

    if (ViewOrientation == Free)
    {
        ViewportCamera.SetNearPlane(0.1f);
        ViewportCamera.SetProjectionType(EViewportProjectionType::Perspective);
        ViewportCamera.SetLocation(FreeCameraLocation);
        ViewportCamera.SetRotation(FreeCameraRotation);
        NavigationController.SetTranslationLocked(false);
        NavigationController.SetRotationLocked(false);
        NavigationController.ToggleHasTargetLocation();
        return;
    }

    if (bWasFree)
    {
        FreeCameraLocation = ViewportCamera.GetLocation();
        FreeCameraRotation = ViewportCamera.GetRotation();
    }

    ViewportCamera.SetNearPlane(-ViewportCamera.GetFarPlane());
    ViewportCamera.SetProjectionType(EViewportProjectionType::Orthographic);
    NavigationController.ToggleHasTargetLocation();
    ViewportCamera.SetOrthoHeight(15.0f);
    switch (ViewOrientation)
    {
    case Top:
    {
        ViewportCamera.SetRotation(FRotator(90.0f, 0.0f, 0.0f));
        ViewportCamera.SetLocation(FVector(0.0f, 0.0f, 100.0f));
        break;
    }
    case Bottom:
    {
        ViewportCamera.SetRotation(FRotator(-90.0f, 0.0f, 0.0f));
        ViewportCamera.SetLocation(FVector(0.0f, 0.0f, -100.0f));
        break;
    }
    case Left:
    {
        ViewportCamera.SetRotation(FRotator(0.0f, 90.0f, 0.0f));
        ViewportCamera.SetLocation(FVector(0.0f, 100.0f, 0.0f));
        break;
    }
    case Right:
    {
        ViewportCamera.SetRotation(FRotator(0.0f, -90.0f, 0.0f));
        ViewportCamera.SetLocation(FVector(0.0f, -100.0f, 0.0f));
        break;
    }
    case Front:
    {
        ViewportCamera.SetRotation(FRotator(0.0f, 0.0f, 0.0f));
        ViewportCamera.SetLocation(FVector(-100.0f, 0.0f, 0.0f));
        break;
    }
    case Back:
    {
        ViewportCamera.SetRotation(FRotator(0.0f, 0.0f, 180.0f));
        ViewportCamera.SetLocation(FVector(100.0f, 0.0f, 0.0f));
        break;
    }
    }

    NavigationController.SetRotationLocked(true);
    //NavigationController.SetTranslationLocked(true);
}

void FEditorViewportClient::SetViewportOrigin(uint32 InOriginX, uint32 InOriginY)
{
    ViewportCamera.SetOrigin(InOriginX, InOriginY);
}

void FEditorViewportClient::UseSharedSelectionController(FViewportSelectionController* Shared)
{
    ActiveController = Shared;
    SelectionInputContext.SetSelectionController(Shared);
    GizmoController.SetViewportSelectionController(Shared);
    NavigationController.SetSelectionController(Shared);
}

void FEditorViewportClient::SetEditorContext(FEditorContext* InContext)
{
    EditorContext = InContext;
    ActiveController->SetEditorContext(InContext);
}

void FEditorViewportClient::SetScene(FScene* InScene)
{
    CurScene = InScene;
    ActiveController->SetActors(InScene != nullptr ? InScene->GetActors() : nullptr);
}

void FEditorViewportClient::SyncSelectionFromContext()
{
    ActiveController->SyncSelectionFromContext();
}

void FEditorViewportClient::DrawViewportOverlay(const ImVec2& ViewPos, const ImVec2& ViewSize)
{
    if (ActiveController->IsDraggingSelection())
    {
        int32 StartX, StartY, EndX, EndY;
        ActiveController->GetSelectionRect(StartX, StartY, EndX, EndY);

        const float MinX = (float)std::min(StartX, EndX);
        const float MinY = (float)std::min(StartY, EndY);
        const float MaxX = (float)std::max(StartX, EndX);
        const float MaxY = (float)std::max(StartY, EndY);

        ImDrawList* DrawList = ImGui::GetForegroundDrawList();
        DrawList->AddRectFilled(ImVec2(MinX, MinY), ImVec2(MaxX, MaxY), IM_COL32(80, 140, 255, 40));
        DrawList->AddRect(ImVec2(MinX, MinY), ImVec2(MaxX, MaxY), IM_COL32(80, 140, 255, 255), 0.0f,
                          0, 1.5f);
    }

    ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (MainViewport == nullptr)
    {
        return;
    }

    ImDrawList* DrawList = ImGui::GetBackgroundDrawList(MainViewport);
    if (DrawList == nullptr)
    {
        return;
    }

    DrawList->PushClipRect(ViewPos, ImVec2(ViewPos.x + ViewSize.x, ViewPos.y + ViewSize.y), true);
    DrawStatOverlay(DrawList, ViewPos, ViewSize);
    DrawList->PopClipRect();
}

void FEditorViewportClient::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewPos,
                                            const ImVec2& ViewSize)
{
    if (DrawList == nullptr || StatFlags == EViewportStatFlags::None)
    {
        return;
    }

    if (IsStatEnabled(EViewportStatFlags::FPS))
    {
        const FFPSStatData FPSData = CollectFPSStatData();
        DrawFPSStatOverlay(FPSData, DrawList, ViewPos, ViewSize);
    }

    if (IsStatEnabled(EViewportStatFlags::Memory))
    {
        const FMemoryStatData MemoryData = CollectMemoryStatData();
        DrawMemoryStatOverlay(MemoryData, DrawList, ViewPos, ViewSize);
    }
}

void FEditorViewportClient::DrawFPSStatOverlay(const FFPSStatData& InData, ImDrawList* DrawList,
                                               const ImVec2& ViewPos, const ImVec2& ViewSize)
{
    if (DrawList == nullptr)
    {
        return;
    }

    const float ViewLeft = ViewPos.x;
    const float ViewTop = ViewPos.y;
    const float ViewWidth = ViewSize.x;
    const float ViewHeight = ViewSize.y;

    char FPSLine[64];
    char FrameTimeLine[64];

    snprintf(FPSLine, sizeof(FPSLine), "%.2f FPS", InData.FPS);
    snprintf(FrameTimeLine, sizeof(FrameTimeLine), "%.2f ms", InData.FrameTimeMS);

    const ImVec2 FPSTextSize = ImGui::CalcTextSize(FPSLine);
    const ImVec2 FrameTextSize = ImGui::CalcTextSize(FrameTimeLine);

    const float LineSpacing = 4.0f;
    const float TotalHeight = FPSTextSize.y + LineSpacing + FrameTextSize.y;

    const float RightPadding = 50.0f;
    const float AnchorX = ViewLeft + ViewWidth - RightPadding;
    const float AnchorY = ViewTop + ViewHeight * 0.25f;

    const float FPSPosX = AnchorX - FPSTextSize.x;
    const float FPSPosY = AnchorY - TotalHeight * 0.5f;

    const float FramePosX = AnchorX - FrameTextSize.x;
    const float FramePosY = FPSPosY + FPSTextSize.y + LineSpacing;

    const ImU32 FPSTextColor = IM_COL32(0, 255, 234, 255);

    ImFont*     Font = ImGui::GetFont();
    const float FontSize = ImGui::GetFontSize() * 1.25f;

    DrawList->AddText(Font, FontSize, ImVec2(FPSPosX, FPSPosY), FPSTextColor, FPSLine);
    DrawList->AddText(Font, FontSize, ImVec2(FramePosX, FramePosY), FPSTextColor, FrameTimeLine);
}

void FEditorViewportClient::DrawMemoryStatOverlay(const FMemoryStatData& InData, ImDrawList* DrawList,
                                                  const ImVec2& ViewPos, const ImVec2& ViewSize)
{
    (void)InData;
    (void)DrawList;
    (void)ViewPos;
    (void)ViewSize;

    // 추가 예정
}

void FEditorViewportClient::DrawOutline() {}

FFPSStatData FEditorViewportClient::CollectFPSStatData() const
{
    FFPSStatData Data;

    if (EditorContext != nullptr)
    {
        Data.FPS = EditorContext->CurrentFPS;
        Data.FrameTimeMS = EditorContext->DeltaTime * 1000.0f;
    }

    return Data;
}

FMemoryStatData FEditorViewportClient::CollectMemoryStatData() const
{
    FMemoryStatData Data;
    return Data;
}
