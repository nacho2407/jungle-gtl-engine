#include "WindowOverlayManager.h"
#include "Editor/EditorContext.h"
#include "Engine/Scene.h"

#include <cmath>

// Default navigation values applied to newly created panels
static float DefaultMoveSpeed     = 100.0f;
static float DefaultRotationSpeed = 0.3f;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int32 RequiredPanelCount(EViewportLayout Layout)
{
    switch (Layout)
    {
    case EViewportLayout::TwoColumn:
    case EViewportLayout::TwoRow:
        return 2;
    case EViewportLayout::ColumnTwoRow:
    case EViewportLayout::TwoRowColumn:
        return 3;
    case EViewportLayout::FourWay:
        return 4;
    default:
        return 1;
    }
}

FString FWindowOverlayManager::GetViewportLayoutString(EViewportLayout Layout) const 
{
    switch (Layout)
    {
    case EViewportLayout::Single:
        return "Single";
    case EViewportLayout::TwoColumn:
        return "TwoColumn";
    case EViewportLayout::TwoRow:
        return "TwoRow";
    case EViewportLayout::ColumnTwoRow:
        return "ColumnTwoRow";
    case EViewportLayout::TwoRowColumn:
        return "TwoRowColumn";
    case EViewportLayout::FourWay:
        return "FourWay";
    default:
        return "Unknown";
    }
}

SSplitter* FWindowOverlayManager::GetSplitterV() const
{
    if (SplitterV)
        return SplitterV;
    else
        return nullptr;
}

SSplitter* FWindowOverlayManager::GetSplitterH() const
{
    if (SplitterH)
        return SplitterH;
    else
        return nullptr;
}

// ---------------------------------------------------------------------------
// Panel dimension management
// ---------------------------------------------------------------------------

void FWindowOverlayManager::ResetViewportDimension()
{
    // 1. Adjust panel count to match the layout (panel[0] is always preserved)
    const int32 Required = RequiredPanelCount(ViewportLayout);

    while (static_cast<int32>(ViewportPanels.size()) > Required)
    {
        FEditorViewportPanel* Panel = ViewportPanels.back();
        if (Panel == LastFocusedPanel)
            LastFocusedPanel = nullptr;
        if (Panel->Viewport)
        {
            delete Panel->Viewport;
        }
        if (Panel->ViewportClient)
        {
            Panel->ViewportClient->Release();
            delete Panel->ViewportClient;
        }
        delete Panel;
        ViewportPanels.pop_back();
    }

    while (static_cast<int32>(ViewportPanels.size()) < Required)
    {
        AddNewViewportPanel();
    }

    // 2. Rebuild splitters (creates SSplitterV/H with the correct panel assignments)
    ResetSplitters();

    // 3. Apply baseline: every panel covers the full window
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (!Panel) continue;
        Panel->PosX   = 0.f;
        Panel->PosY   = 0.f;
        Panel->Width  = static_cast<float>(W);
        Panel->Height = static_cast<float>(H);
    }

    // 4. Each splitter narrows its own axis independently
    if (SplitterV) SplitterV->ResetPanelDimension();
    if (SplitterH) SplitterH->ResetPanelDimension();

    // 5. Push final positions to every ViewportClient
    SyncPanelClients();

    // 6. Set default oreintation values for each panel
    ResetViewOrientation();
}

void FWindowOverlayManager::SyncPanelClients()
{
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (!Panel || !Panel->ViewportClient) continue;
        Panel->ViewportClient->OnResize(
            static_cast<uint32>(Panel->Width),
            static_cast<uint32>(Panel->Height));
        Panel->ViewportClient->SetViewportOrigin(
            static_cast<uint32>(Panel->PosX),
            static_cast<uint32>(Panel->PosY));
    }
}

// ---------------------------------------------------------------------------
// Splitter management
// ---------------------------------------------------------------------------

void FWindowOverlayManager::ResetSplitters()
{
    ReleaseSplitters();

    if (ViewportLayout == EViewportLayout::Single) return;

    const float FW    = static_cast<float>(W);
    const float FH    = static_cast<float>(H);
    const float SplitX = FW * VSplitRatio;
    const float SplitY = FH * HSplitRatio;

    switch (ViewportLayout)
    {
    case EViewportLayout::TwoColumn:
        SplitterV = new SSplitterV();
        SplitterV->Init(SplitX, 0.f, SplitterHalfThick * 2.f, FH, FW, FH);
        SplitterV->SetLeftPanels ({ViewportPanels[0]});
        SplitterV->SetRightPanels({ViewportPanels[1]});
        break;

    case EViewportLayout::TwoRow:
        SplitterH = new SSplitterH();
        SplitterH->Init(0.f, SplitY, FW, SplitterHalfThick * 2.f, FW, FH);
        SplitterH->SetUpPanels    ({ViewportPanels[0]});
        SplitterH->SetBottomPanels({ViewportPanels[1]});
        break;

    case EViewportLayout::ColumnTwoRow:
        SplitterV = new SSplitterV();
        SplitterV->Init(SplitX, 0.f, SplitterHalfThick * 2.f, FH, FW, FH);
        SplitterV->SetLeftPanels ({ViewportPanels[0]});
        SplitterV->SetRightPanels({ViewportPanels[1], ViewportPanels[2]});

        SplitterH = new SSplitterH();
        SplitterH->Init(SplitX, SplitY, FW - SplitX, SplitterHalfThick * 2.f, FW, FH);
        SplitterH->SetUpPanels    ({ViewportPanels[1]});
        SplitterH->SetBottomPanels({ViewportPanels[2]});
        break;

    case EViewportLayout::TwoRowColumn:
        SplitterV = new SSplitterV();
        SplitterV->Init(SplitX, 0.f, SplitterHalfThick * 2.f, FH, FW, FH);
        SplitterV->SetLeftPanels ({ViewportPanels[0], ViewportPanels[1]});
        SplitterV->SetRightPanels({ViewportPanels[2]});

        SplitterH = new SSplitterH();
        SplitterH->Init(0.f, SplitY, SplitX, SplitterHalfThick * 2.f, FW, FH);
        SplitterH->SetUpPanels    ({ViewportPanels[0]});
        SplitterH->SetBottomPanels({ViewportPanels[1]});
        break;

    case EViewportLayout::FourWay:
        SplitterV = new SSplitterV();
        SplitterV->Init(SplitX, 0.f, SplitterHalfThick * 2.f, FH, FW, FH);
        SplitterV->SetLeftPanels ({ViewportPanels[0], ViewportPanels[1]});
        SplitterV->SetRightPanels({ViewportPanels[2], ViewportPanels[3]});

        SplitterH = new SSplitterH();
        SplitterH->Init(0.f, SplitY, FW, SplitterHalfThick * 2.f, FW, FH);
        SplitterH->SetUpPanels    ({ViewportPanels[0], ViewportPanels[2]});
        SplitterH->SetBottomPanels({ViewportPanels[1], ViewportPanels[3]});
        break;

    default:
        break;
    }
}

void FWindowOverlayManager::ReleaseSplitters()
{
    delete SplitterV; SplitterV = nullptr;
    delete SplitterH; SplitterH = nullptr;
}

// ---------------------------------------------------------------------------
// Splitter drag interaction
// ---------------------------------------------------------------------------

bool FWindowOverlayManager::HitTestSplitterV(int32 X, int32 Y) const
{
    if (!SplitterV) return false;
    return SplitterV->HitTest(FVector2(static_cast<float>(X), static_cast<float>(Y)));
}

bool FWindowOverlayManager::HitTestSplitterH(int32 X, int32 Y) const
{
    if (!SplitterH) return false;
    return SplitterH->HitTest(FVector2(static_cast<float>(X), static_cast<float>(Y)));
}

void FWindowOverlayManager::BeginSplitterDrag(bool bVertical, bool bHorizontal)
{
    bDraggingV = bVertical   && SplitterV != nullptr;
    bDraggingH = bHorizontal && SplitterH != nullptr;
}

void FWindowOverlayManager::UpdateSplitterDrag(float DeltaX, float DeltaY)
{
    const float FW = static_cast<float>(W);
    const float FH = static_cast<float>(H);

    if (bDraggingV && SplitterV)
    {
        SplitterV->OnDrag(DeltaX);
        VSplitRatio = SplitterV->PosX / FW;

        if (SplitterH)
        {
            switch (ViewportLayout)
            {
            case EViewportLayout::ColumnTwoRow:
                SplitterH->PosX  = SplitterV->PosX;
                SplitterH->Width = FW - SplitterV->PosX;
                break;
            case EViewportLayout::TwoRowColumn:
                SplitterH->Width = SplitterV->PosX;
                break;
            default:
                break;
            }
        }

        SyncPanelClients();
    }

    if (bDraggingH && SplitterH)
    {
        SplitterH->OnDrag(DeltaY);
        HSplitRatio = SplitterH->PosY / FH;
        SyncPanelClients();
    }
}

void FWindowOverlayManager::EndSplitterDrag()
{
    bDraggingV = false;
    bDraggingH = false;
}

// ---------------------------------------------------------------------------
// Panel accessors
// ---------------------------------------------------------------------------

TArray<FEditorViewportPanel*>& FWindowOverlayManager::GetViewportPanels() { return ViewportPanels; }

FEditorViewportPanel* FWindowOverlayManager::FindPanelAtPoint(int32 X, int32 Y)
{
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (!Panel) continue;
        if (Panel->HitTest(FVector2(static_cast<float>(X), static_cast<float>(Y))))
        {
            return Panel;
        }
    }
    return nullptr;
}

FEditorViewportPanel* FWindowOverlayManager::FindPanelAtPointClicked(int32 X, int32 Y)
{
    auto* Panel = FindPanelAtPoint(X, Y);
    LastFocusedPanel = Panel;
    return Panel;
}

void FWindowOverlayManager::AddNewViewportPanel()
{
    FEditorViewportPanel*  Panel         = new FEditorViewportPanel();
    FEditorViewportClient* ViewportClient = new FEditorViewportClient();
    ViewportClient->Create();
    ViewportClient->SetEditorContext(EditorContext);
    ViewportClient->SetScene(Scene);
    ViewportClient->GetNavigationController().SetMoveSpeed(DefaultMoveSpeed);
    ViewportClient->GetNavigationController().SetRotationSpeed(DefaultRotationSpeed);

    Panel->ViewportClient = ViewportClient;
    ViewportClient->OnPickRequested = PickCallback;
    ViewportPanels.push_back(Panel);
}

void FWindowOverlayManager::SetViewportOrientation(EViewportViewOrientation InViewOrientation) 
{
    if (LastFocusedPanel && LastFocusedPanel->ViewportClient)
    {
        LastFocusedPanel->ViewportClient->SetViewOrientation(InViewOrientation);
    }
}

void FWindowOverlayManager::ResetViewOrientation()
{
    using enum EViewportLayout;
    using enum EViewportViewOrientation;
    if (ViewportPanels.empty()) return;

    switch (ViewportLayout)
    {
    case Single:
    {
        if (ViewportPanels[0] && ViewportPanels[0]->ViewportClient)
            ViewportPanels[0]->ViewportClient->SetViewOrientation(Free);
        break;
    }
    case TwoColumn:
    {
        for (uint32 i = 0; i < ViewportPanels.size(); i++)
        {
            auto* Panel = ViewportPanels[i];
            if (Panel && Panel->ViewportClient)
            {
                if (i == 0)
                    Panel->ViewportClient->SetViewOrientation(Free);
                if (i == 1)
                    Panel->ViewportClient->SetViewOrientation(Top);
            }
        }
        break;
    }
    case TwoRow:
    {
        for (uint32 i = 0; i < ViewportPanels.size(); i++)
        {
            auto* Panel = ViewportPanels[i];
            if (Panel && Panel->ViewportClient)
            {
                if (i == 0)
                    Panel->ViewportClient->SetViewOrientation(Free);
                if (i == 1)
                    Panel->ViewportClient->SetViewOrientation(Front);
            }
        }
        break;
    }
    case ColumnTwoRow:
    {
        for (uint32 i = 0; i < ViewportPanels.size(); i++)
        {
            auto* Panel = ViewportPanels[i];
            if (Panel && Panel->ViewportClient)
            {
                if (i == 0)
                    Panel->ViewportClient->SetViewOrientation(Free);
                if (i == 1)
                    Panel->ViewportClient->SetViewOrientation(Top);
                if (i == 2)
                    Panel->ViewportClient->SetViewOrientation(Front);
            }
        }
        break;
    }
    case TwoRowColumn:
    {
        for (uint32 i = 0; i < ViewportPanels.size(); i++)
        {
            auto* Panel = ViewportPanels[i];
            if (Panel && Panel->ViewportClient)
            {
                if (i == 0)
                    Panel->ViewportClient->SetViewOrientation(Free);
                if (i == 1)
                    Panel->ViewportClient->SetViewOrientation(Top);
                if (i == 2)
                    Panel->ViewportClient->SetViewOrientation(Front);
            }
        }
        break;
    }
    case FourWay:
    {
        for (uint32 i = 0; i < ViewportPanels.size(); i++)
        {
            auto* Panel = ViewportPanels[i];
            if (Panel && Panel->ViewportClient)
            {
                if (i == 0)
                    Panel->ViewportClient->SetViewOrientation(Free);
                if (i == 1)
                    Panel->ViewportClient->SetViewOrientation(Top);
                if (i == 2)
                    Panel->ViewportClient->SetViewOrientation(Front);
                if (i == 3)
                    Panel->ViewportClient->SetViewOrientation(Right);
            }
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Scene / context / settings
// ---------------------------------------------------------------------------

void FWindowOverlayManager::SetScene(FScene* InScene)
{
    Scene = InScene;
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (Panel && Panel->ViewportClient)
            Panel->ViewportClient->SetScene(InScene);
    }
}

FSceneFrameRenderData FWindowOverlayManager::BuildSceneFrameData() const
{
    FSceneFrameRenderData FrameData;
    if (Scene == nullptr)
    {
        return FrameData;
    }

    ESceneShowFlags UnionFlags = ESceneShowFlags::None;
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (Panel && Panel->ViewportClient)
            UnionFlags |= Panel->ViewportClient->GetRenderSetting().BuildSceneShowFlags();
    }

    Scene->BuildRenderData(FrameData, UnionFlags);
    return FrameData;
}

void FWindowOverlayManager::SetPickCallback(FEditorViewportClient::FPickCallback Callback)
{
    PickCallback = std::move(Callback);
    for (FEditorViewportPanel* Panel : ViewportPanels)
    {
        if (Panel && Panel->ViewportClient)
            Panel->ViewportClient->OnPickRequested = PickCallback;
    }
}

void FWindowOverlayManager::Release()
{
    LastFocusedPanel = nullptr;
    ReleaseSplitters();
    for (uint32 i = 0; i < ViewportPanels.size(); ++i)
    {
        FEditorViewportPanel* Panel = ViewportPanels[i];
        if (Panel)
        {
            if (i > 0) // Index 0 is owned by FEditor
            {
                if (Panel->Viewport)
                {
                    delete Panel->Viewport;
                    Panel->Viewport = nullptr;
                }
                if (Panel->ViewportClient)
                {
                    Panel->ViewportClient->Release();
                    delete Panel->ViewportClient;
                    Panel->ViewportClient = nullptr;
                }
            }
            delete Panel;
        }
    }
}

void FWindowOverlayManager::SetWindowDimension(uint32 Width, uint32 Height)
{
    W = Width;
    H = Height;
    ResetViewportDimension();
}

void FWindowOverlayManager::SetViewportLayout(EViewportLayout Layout)
{
    ViewportLayout = Layout;
    VSplitRatio    = 0.5f;
    HSplitRatio    = 0.5f;
    ResetViewportDimension();
}

void FWindowOverlayManager::SetNavigationValues(float MoveSpeed, float RotationSpeed)
{
    DefaultMoveSpeed     = MoveSpeed;
    DefaultRotationSpeed = RotationSpeed;

    // Apply retroactively to panels[1+] (panel[0] is owned by FEditor)
    for (uint32 i = 1; i < ViewportPanels.size(); ++i)
    {
        FEditorViewportPanel* Panel = ViewportPanels[i];
        if (Panel && Panel->ViewportClient)
        {
            Panel->ViewportClient->GetNavigationController().SetMoveSpeed(MoveSpeed);
            Panel->ViewportClient->GetNavigationController().SetRotationSpeed(RotationSpeed);
        }
    }
}

void FWindowOverlayManager::BuildViewportWIdgetData(FWidgetRenderData& WidgetData)
{
    if (SplitterV)
    {
        WidgetData.Widgets.push_back(SplitterV);
    }
    if (SplitterH)
    {
        WidgetData.Widgets.push_back(SplitterH);
    }
    WidgetData.ScreenWidth = W;
    WidgetData.ScreenHeight = H;
}