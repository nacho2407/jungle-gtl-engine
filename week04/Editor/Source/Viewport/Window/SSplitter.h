#pragma once
#include "Core/CoreMinimal.h"
#include "EditorViewportPanel.h"

class SSplitter
{
protected:
    SSplitter() = default;
    FVector2 Origin       = FVector2(0, 0);
    float    Extent       = 0;
    float    WindowWidth  = 0;
    float    WindowHeight = 0;

    // Splitter drag thresholds
    //float    ScreenMin = 0;
    //float    ScreenMax = 0;

public:
    // Responds to window resizing
    void OnResize(float Width, float Height);

    // Relocate splitter to match drag event. Should also prevent window overlaps with bounds
    virtual void OnDrag(float Delta, float MinBound, float MaxBound) = 0;

    // Resize panels accordingly to splitter origin
    virtual void ResetPanelDimension() = 0;

    // TODO:: Add render related stuffs here. Do NOT use ImGui, or features unrelated to DX11

    virtual ~SSplitter() = default;
};

class SSplitterV : public SSplitter
{
private:
    TArray<FEditorViewportPanel*> LeftPanels;
    TArray<FEditorViewportPanel*> RightPanels;

public:
    void OnDrag(float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;
    ~SSplitterV() override = default;
};

class SSplitterH : public SSplitter
{
private:
    TArray<FEditorViewportPanel*> UpPanels;
    TArray<FEditorViewportPanel*> BottomPanels;

public:
    void OnDrag(float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;
    ~SSplitterH() override = default;
};