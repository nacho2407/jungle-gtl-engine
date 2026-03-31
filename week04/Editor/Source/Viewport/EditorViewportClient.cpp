#include "Viewport/EditorViewportClient.h"

#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <imgui.h>

#include "ApplicationCore/Input/InputRouter.h"
#include "Core/HAL/PlatformMemory.h"
#include "Editor/EditorContext.h"
#include "Engine/EngineStatics.h"
#include "Engine/Scene.h"
#include "Engine/Game/Actor.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/MemoryTracker.h"

namespace
{
    FString FormatBytes(uint64 Bytes)
    {
        constexpr double KB = 1024.0;
        constexpr double MB = KB * 1024.0;
        constexpr double GB = MB * 1024.0;
        constexpr double TB = GB * 1024.0;

        char Buffer[64] = {};

        if (Bytes >= static_cast<uint64>(TB))
        {
            snprintf(Buffer, sizeof(Buffer), "%.2f TB", static_cast<double>(Bytes) / TB);
        }
        else if (Bytes >= static_cast<uint64>(GB))
        {
            snprintf(Buffer, sizeof(Buffer), "%.2f GB", static_cast<double>(Bytes) / GB);
        }
        else if (Bytes >= static_cast<uint64>(MB))
        {
            snprintf(Buffer, sizeof(Buffer), "%.2f MB", static_cast<double>(Bytes) / MB);
        }
        else if (Bytes >= static_cast<uint64>(KB))
        {
            snprintf(Buffer, sizeof(Buffer), "%.2f KB", static_cast<double>(Bytes) / KB);
        }
        else
        {
            snprintf(Buffer, sizeof(Buffer), "%llu B", static_cast<unsigned long long>(Bytes));
        }

        return FString(Buffer);
    }
}

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
    if (DrawList == nullptr || InData.Rows.empty())
    {
        return;
    }

    const ImVec2 Padding(10.0f, 8.0f);
    const ImVec2 PanelOffset(18.0f, 18.0f);
    const float  RowHeight = 22.0f;
    const float  LabelValueGap = 24.0f;
    const float  MinPanelWidth = 320.0f;

    const ImU32 PanelBgColor = IM_COL32(8, 12, 10, 190);
    const ImU32 RowBgEvenColor = IM_COL32(18, 34, 22, 170);
    const ImU32 RowBgOddColor = IM_COL32(26, 48, 32, 170);
    const ImU32 BorderColor = IM_COL32(90, 180, 110, 180);
    const ImU32 LabelColor = IM_COL32(170, 255, 180, 255);
    const ImU32 ValueColor = IM_COL32(120, 255, 140, 255);

    ImVec2 PanelMin(ViewPos.x + PanelOffset.x, ViewPos.y + PanelOffset.y);

    float MaxLabelWidth = 0.0f;
    float MaxValueWidth = 0.0f;

    for (const FMemoryStatRow& Row : InData.Rows)
    {
        MaxLabelWidth = std::max(MaxLabelWidth, ImGui::CalcTextSize(Row.Label.c_str()).x);
        MaxValueWidth = std::max(MaxValueWidth, ImGui::CalcTextSize(Row.Value.c_str()).x);
    }

    const float PanelWidth =
        std::max(MinPanelWidth, Padding.x * 2.0f + MaxLabelWidth + LabelValueGap + MaxValueWidth);
    const float PanelHeight = Padding.y * 2.0f + RowHeight * static_cast<float>(InData.Rows.size());

    ImVec2 PanelMax(PanelMin.x + PanelWidth, PanelMin.y + PanelHeight);

    // viewport 밖으로 넘치지 않게 살짝 보정
    if (PanelMax.x > ViewPos.x + ViewSize.x - 8.0f)
    {
        const float ShiftX = PanelMax.x - (ViewPos.x + ViewSize.x - 8.0f);
        PanelMin.x -= ShiftX;
        PanelMax.x -= ShiftX;
    }

    if (PanelMax.y > ViewPos.y + ViewSize.y - 8.0f)
    {
        const float ShiftY = PanelMax.y - (ViewPos.y + ViewSize.y - 8.0f);
        PanelMin.y -= ShiftY;
        PanelMax.y -= ShiftY;
    }

    DrawList->AddRectFilled(PanelMin, PanelMax, PanelBgColor, 6.0f);
    DrawList->AddRect(PanelMin, PanelMax, BorderColor, 6.0f, 0, 1.0f);

    const float LabelX = PanelMin.x + Padding.x;
    const float ValueX = PanelMax.x - Padding.x;

    for (int32 RowIndex = 0; RowIndex < static_cast<int32>(InData.Rows.size()); ++RowIndex)
    {
        const FMemoryStatRow& Row = InData.Rows[RowIndex];

        const float RowTop = PanelMin.y + Padding.y + RowHeight * static_cast<float>(RowIndex);
        const float RowBottom = RowTop + RowHeight;
        const ImU32 RowColor = (RowIndex % 2 == 0) ? RowBgEvenColor : RowBgOddColor;

        DrawList->AddRectFilled(ImVec2(PanelMin.x + 4.0f, RowTop),
                                ImVec2(PanelMax.x - 4.0f, RowBottom), RowColor, 3.0f);

        const ImVec2 LabelSize = ImGui::CalcTextSize(Row.Label.c_str());
        const ImVec2 ValueSize = ImGui::CalcTextSize(Row.Value.c_str());

        const float TextY = RowTop + (RowHeight - LabelSize.y) * 0.5f;
        const float ValueTextX = ValueX - ValueSize.x;

        DrawList->AddText(ImVec2(LabelX, TextY), LabelColor, Row.Label.c_str());
        DrawList->AddText(ImVec2(ValueTextX, TextY), ValueColor, Row.Value.c_str());
    }
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

    FMemorySnapshot Snapshot = GMemoryTracker.CaptureSnapshot();

    Data.Rows.push_back({"Heap Allocated Bytes (Engine)", FormatBytes(Snapshot.HeapAllocatedBytes)});
    Data.Rows.push_back({"Heap Allocation Count (Engine)", std::to_string(Snapshot.HeapAllocationCount)});

    Data.Rows.push_back({"Total Physical Memory", FormatBytes(Snapshot.Platform.TotalPhysical)});
    Data.Rows.push_back({"Available Physical Memory", FormatBytes(Snapshot.Platform.AvailablePhysical)});
    Data.Rows.push_back({"Total Virtual Memory", FormatBytes(Snapshot.Platform.TotalVirtual)});
    Data.Rows.push_back({"Available Virtual Memory", FormatBytes(Snapshot.Platform.AvailableVirtual)});

    Data.Rows.push_back({"Tracked Texture Memory", FormatBytes(Snapshot.TrackedGpu.TextureBytes)});
    Data.Rows.push_back({"Tracked Render Target Memory", FormatBytes(Snapshot.TrackedGpu.RenderTargetBytes)});
    Data.Rows.push_back({"Tracked Depth Stencil Memory", FormatBytes(Snapshot.TrackedGpu.DepthStencilBytes)});
    Data.Rows.push_back({"Tracked Vertex Buffer Memory", FormatBytes(Snapshot.TrackedGpu.VertexBufferBytes)});
    Data.Rows.push_back({"Tracked Index Buffer Memory", FormatBytes(Snapshot.TrackedGpu.IndexBufferBytes)});
    Data.Rows.push_back({"Tracked Vertex Shader Memory", FormatBytes(Snapshot.TrackedGpu.VertexShaderBlobBytes)});
    Data.Rows.push_back({"Tracked Pixel Shader Memory", FormatBytes(Snapshot.TrackedGpu.PixelShaderBlobBytes)});

    Data.Rows.push_back({"Tracked Total GPU Memory", FormatBytes(Snapshot.TrackedGpu.TotalTrackedGpuBytes())});

    return Data;
}
