#pragma once
#include "Core/Runtime/Slate/Window/SWindow.h"

class ENGINE_API SSplitter : public SWindow
{
  protected:
    SSplitter() = default;
    float    WindowWidth  = 0;
    float    WindowHeight = 0;

  public:
    void Init(float X, float Y, float InWidth, float InHeight, float InWindowWidth, float InWindowHeight)
    {
        PosX         = X;
        PosY         = Y;
        Width        = InWidth;
        Height       = InHeight;
        WindowWidth  = InWindowWidth;
        WindowHeight = InWindowHeight;
    }

    void OnResize(float Width, float Height);

    virtual void OnDrag(float Delta, float MinBound, float MaxBound) = 0;
    virtual void ResetPanelDimension() = 0;

    bool HitTest(FVector2 P) const override { return false; }

    float    GetWindowWidth()  const { return WindowWidth; }
    float    GetWindowHeight() const { return WindowHeight; }

    virtual ~SSplitter() = default;
};

class ENGINE_API SSplitterV : public SSplitter
{
  private:
    TArray<SWindow*> LeftPanels;
    TArray<SWindow*> RightPanels;

  public:
    void SetLeftPanels (TArray<SWindow*> InPanels) { LeftPanels  = std::move(InPanels); }
    void SetRightPanels(TArray<SWindow*> InPanels) { RightPanels = std::move(InPanels); }

    void OnMouseButtonDown  (int32 X, int32 Y) override;
    void OnMouseButtonUp    (int32 X, int32 Y) override;
    void OnMouseMove        (float DeltaX, float DeltaY) override;
    void OnDrag             (float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;

    ~SSplitterV() override = default;
};

class ENGINE_API SSplitterH : public SSplitter
{
  private:
    TArray<SWindow*> UpPanels;
    TArray<SWindow*> BottomPanels;

  public:
    void SetUpPanels    (TArray<SWindow*> InPanels) { UpPanels     = std::move(InPanels); }
    void SetBottomPanels(TArray<SWindow*> InPanels) { BottomPanels = std::move(InPanels); }

    void OnMouseButtonDown  (int32 X, int32 Y) override;
    void OnMouseButtonUp    (int32 X, int32 Y) override;
    void OnMouseMove        (float DeltaX, float DeltaY) override;
    void OnDrag             (float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;

    ~SSplitterH() override = default;
};
