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

public:
    // Initialise origin, extent, and window size in one call
    void Init(FVector2 InOrigin, float InExtent, float InWindowWidth, float InWindowHeight)
    {
        Origin       = InOrigin;
        Extent       = InExtent;
        WindowWidth  = InWindowWidth;
        WindowHeight = InWindowHeight;
    }

    // Responds to window resizing
    void OnResize(float Width, float Height);

    // Relocate splitter to match drag event. Should also prevent window overlaps with bounds
    virtual void OnDrag(float Delta, float MinBound, float MaxBound) = 0;

    // Resize panels accordingly to splitter origin and extent
    virtual void ResetPanelDimension() = 0;

    FVector2 GetOrigin()      const { return Origin; }
    float    GetWindowWidth() const { return WindowWidth; }
    float    GetWindowHeight()const { return WindowHeight; }
    float    GetExtent()      const { return Extent; }

    // TODO:: Add render related stuffs here. Do NOT use ImGui, or features unrelated to DX11

    virtual ~SSplitter() = default;
};

class SSplitterV : public SSplitter
{
private:
    TArray<FEditorViewportPanel*> LeftPanels;
    TArray<FEditorViewportPanel*> RightPanels;

public:
    void SetLeftPanels (TArray<FEditorViewportPanel*> InPanels) { LeftPanels  = std::move(InPanels); }
    void SetRightPanels(TArray<FEditorViewportPanel*> InPanels) { RightPanels = std::move(InPanels); }

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
    void SetUpPanels    (TArray<FEditorViewportPanel*> InPanels) { UpPanels     = std::move(InPanels); }
    void SetBottomPanels(TArray<FEditorViewportPanel*> InPanels) { BottomPanels = std::move(InPanels); }

    void OnDrag(float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;
    ~SSplitterH() override = default;
};
