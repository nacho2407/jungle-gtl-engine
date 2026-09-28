#pragma once
#include "Core/CoreTypes.h"
#include "Geometry/Ray.h"

class FEditorViewportClient;
class FEditorSettings;

class IBaseEditorController
{
  public:
    virtual ~IBaseEditorController() = default;
    virtual void OnMouseMove(float DeltaX, float DeltaY) = 0;
    virtual void OnMouseMoveAbsolute(float X, float Y) {}
    virtual void OnLeftMouseClick(float X, float Y) = 0;    // LMB down
    virtual void OnLeftMouseDragEnd(float X, float Y) = 0;  // LMB drag released
    virtual void OnLeftMouseButtonUp(float X, float Y) = 0; // LMB up (no drag)
    virtual void OnRightMouseClick(float DeltaX, float DeltaY) = 0;
    virtual void OnLeftMouseDrag(float X, float Y) = 0; // drag in progress (X/Y = viewport-local pos)
    virtual void OnRightMouseDrag(float DeltaX, float DeltaY) = 0;
    virtual void OnMiddleMouseDrag(float DeltaX, float DeltaY) = 0;
    virtual void OnKeyPressed(int VK) = 0;
    virtual void OnKeyDown(int VK) = 0;
    virtual void OnKeyReleased(int VK) = 0;
    virtual void OnWheelScrolled(float Notch) = 0;
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