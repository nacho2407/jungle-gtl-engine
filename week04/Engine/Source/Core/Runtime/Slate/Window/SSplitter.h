#pragma once
#include "Core/Runtime/Slate/Window/SWindow.h"

class ENGINE_API SSplitter : public SWindow
{
  protected:
    SSplitter() = default;
    float    MinBound    = 0;
    float    MaxBound    = 0;
    float    WindowWidth = 0;
    float    WindowHeight= 0;
    bool     bIsClicked  = false;

    float AreaMinX = 0.0f;
    float AreaMaxX = 0.0f;
    float AreaMinY = 0.0f;
    float AreaMaxY = 0.0f;

  public:
    void Init(float X, float Y, float InWidth, float InHeight, float InWindowWidth, float InWindowHeight)
    {
        PosX          = X;
        PosY          = Y;
        Width         = InWidth;
        Height        = InHeight;
        WindowWidth   = InWindowWidth;
        WindowHeight  = InWindowHeight;
        MinBound      = 0.f;
        MaxBound      = 0.f;
    }

    void SetDragBounds(float InMin, float InMax) { MinBound = InMin; MaxBound = InMax; }

    void         OnResize(float Width, float Height);
    void         OnMouseButtonDown(int32 X, int32 Y) override;
    void         OnMouseButtonUp(int32 X, int32 Y) override;
    void         OnMouseMove(float DeltaX, float DeltaY) override;
    virtual void OnDrag(float Delta) = 0;
    virtual void ResetPanelDimension() = 0;

    float GetMinBound()    const { return MinBound; }
    float GetMaxBound()    const { return MaxBound; }
    float GetWindowWidth() const { return WindowWidth; }
    float GetWindowHeight()const { return WindowHeight; }

    virtual ~SSplitter() = default;

    void SetPanelAreaBounds(float InMinX, float InMaxX, float InMinY, float InMaxY)
    {
        AreaMinX = InMinX;
        AreaMaxX = InMaxX;
        AreaMinY = InMinY;
        AreaMaxY = InMaxY;
    }
};

class ENGINE_API SSplitterV : public SSplitter
{
  private:
    TArray<SWindow*> LeftPanels;
    TArray<SWindow*> RightPanels;

  public:
    void SetLeftPanels      (TArray<SWindow*> InPanels) { LeftPanels  = std::move(InPanels); }
    void SetRightPanels     (TArray<SWindow*> InPanels) { RightPanels = std::move(InPanels); }
    void OnDrag             (float Delta) override;
    void ResetPanelDimension() override;

    ~SSplitterV() override = default;
};

class ENGINE_API SSplitterH : public SSplitter
{
  private:
    TArray<SWindow*> UpPanels;
    TArray<SWindow*> BottomPanels;

  public:
    void SetUpPanels        (TArray<SWindow*> InPanels) { UpPanels     = std::move(InPanels); }
    void SetBottomPanels    (TArray<SWindow*> InPanels) { BottomPanels = std::move(InPanels); }
    void OnDrag             (float Delta) override;
    void ResetPanelDimension() override;

    ~SSplitterH() override = default;
};
