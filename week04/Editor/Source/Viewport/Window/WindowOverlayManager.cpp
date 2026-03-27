#include "WindowOverlayManager.h"
#include "Engine/Scene.h"
#include "Editor/EditorContext.h"

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

void FWindowOverlayManager::ResetViewportDimension()
{
    // 1. Compute layout rects
    FRect         PlaceHolder(0, 0, 0, 0);
    TArray<FRect> ViewportDim(4, PlaceHolder);
    if (ViewportLayout == EViewportLayout::Single)
    {
        ViewportDim[0] = FRect(0, 0, W, H);
    }
    else if (ViewportLayout == EViewportLayout::TwoColumn)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H);
        ViewportDim[1] = FRect(W / 2, 0, W / 2, H);
    }
    else if (ViewportLayout == EViewportLayout::TwoRow)
    {
        ViewportDim[0] = FRect(0, 0, W, H / 2);
        ViewportDim[1] = FRect(0, H / 2, W, H / 2);
    }
    else if (ViewportLayout == EViewportLayout::ColumnTwoRow)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H);
        ViewportDim[1] = FRect(W / 2, 0, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, H / 2, W / 2, H / 2);
    }
    else if (ViewportLayout == EViewportLayout::TwoRowColumn)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H / 2);
        ViewportDim[1] = FRect(0, H / 2, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, 0, W / 2, H);
    }
    else if (ViewportLayout == EViewportLayout::FourWay)
    {
        ViewportDim[0] = FRect(0, 0, W / 2, H / 2);
        ViewportDim[1] = FRect(0, H / 2, W / 2, H / 2);
        ViewportDim[2] = FRect(W / 2, 0, W / 2, H / 2);
        ViewportDim[3] = FRect(W / 2, H / 2, W / 2, H / 2);
    }

    // 2. Adjust panel count to match the layout (panel[0] is always preserved)
    const int32 Required = RequiredPanelCount(ViewportLayout);

    while (static_cast<int32>(ViewportPanels.size()) > Required)
    {
        FEditorViewportPanel* Panel = ViewportPanels.back();
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

    // 3. Apply rects to every panel
    for (int32 i = 0; i < static_cast<int32>(ViewportPanels.size()); ++i)
    {
        if (ViewportPanels[i])
        {
            ViewportPanels[i]->PosX   = static_cast<float>(ViewportDim[i].X);
            ViewportPanels[i]->PosY   = static_cast<float>(ViewportDim[i].Y);
            ViewportPanels[i]->Width  = static_cast<float>(ViewportDim[i].W);
            ViewportPanels[i]->Height = static_cast<float>(ViewportDim[i].H);
        }
    }
}

TArray<FEditorViewportPanel*>& FWindowOverlayManager::GetViewportPanels() { return ViewportPanels; }

void FWindowOverlayManager::AddNewViewportPanel() {
    FEditorViewportPanel*  Panel = new FEditorViewportPanel();
    FEditorViewportClient* ViewportClient = new FEditorViewportClient();
    ViewportClient->Create();
    ViewportClient->SetEditorContext(EditorContext);
    ViewportClient->SetScene(Scene);

    Panel->ViewportClient = ViewportClient;
    ViewportPanels.push_back(Panel);
}

void FWindowOverlayManager::Release()
{
    for (uint32 i = 0; i < ViewportPanels.size(); i++)
    {
        FEditorViewportPanel* Panel = ViewportPanels[i]; 
        if (Panel)
        {
            // Index 0 is dependent on Editor
            if (i > 0)
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
    ResetViewportDimension();
}