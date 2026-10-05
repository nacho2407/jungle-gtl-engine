#pragma once
#include "Core/CoreTypes.h"
#include "Geometry/Ray.h"

class FEditorViewportClient;
class FEditorSettings;

class IBaseEditorController
{
  public:
    virtual ~IBaseEditorController() = default;
    virtual void OnMouseMove(float, float) {}
    virtual void OnMouseMoveAbsolute(float, float) {}
    virtual void OnLeftMouseClick(float, float) {}    // LMB down
    virtual void OnLeftMouseDragEnd(float, float) {}  // LMB drag released
    virtual void OnLeftMouseButtonUp(float, float) {} // LMB up (no drag)
    virtual void OnRightMouseClick(float, float) {}
    virtual void OnLeftMouseDrag(float, float) {} // drag in progress (X/Y = viewport-local pos)
    virtual void OnRightMouseDrag(float, float) {}
    virtual void OnMiddleMouseDrag(float, float) {}
    virtual void OnKeyPressed(int) {}
    virtual void OnKeyDown(int) {}
    virtual void OnKeyReleased(int) {}
    virtual void OnWheelScrolled(float) {}
	virtual void Tick(float InDeltaTime) { DeltaTime = InDeltaTime; }

    void SetViewportDim(float X, float Y, float Width, float Height);

  protected:
    IBaseEditorController() = default;
    float ViewportX = 0;
    float ViewportY = 0;
    float ViewportWidth = 0;
    float ViewportHeight = 0;

  protected:
    float DeltaTime = 0;
};
