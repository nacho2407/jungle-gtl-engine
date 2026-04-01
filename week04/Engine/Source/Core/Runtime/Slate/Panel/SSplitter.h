#pragma once
#include "Core/Runtime/Slate/Panel/SPanel.h"
#include "Core/Runtime/Slate/Window/SWindow.h"

class ENGINE_API SSplitter : public SPanel
{
  protected:
    SSplitter() = default;
    FVector2 Origin       = FVector2(0, 0);
    float    Extent       = 0;
    float    WindowWidth  = 0;
    float    WindowHeight = 0;

  public:
    void Init(FVector2 InOrigin, float InExtent, float InWindowWidth, float InWindowHeight)
    {
        Origin       = InOrigin;
        Extent       = InExtent;
        WindowWidth  = InWindowWidth;
        WindowHeight = InWindowHeight;
    }

    void OnResize(float Width, float Height);

    virtual void OnDrag(float Delta, float MinBound, float MaxBound) = 0;
    virtual void ResetPanelDimension() = 0;

    FVector2 GetOrigin()       const { return Origin; }
    float    GetWindowWidth()  const { return WindowWidth; }
    float    GetWindowHeight() const { return WindowHeight; }
    float    GetExtent()       const { return Extent; }

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

    void OnDrag(float Delta, float MinBound, float MaxBound) override;
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

    void OnDrag(float Delta, float MinBound, float MaxBound) override;
    void ResetPanelDimension() override;
    ~SSplitterH() override = default;
};
