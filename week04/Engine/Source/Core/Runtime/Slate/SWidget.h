#pragma once
#include "Core/CoreMinimal.h"

class ENGINE_API SWidget
{
  public:
    //virtual void Render(ID3D11DeviceContext* Context) = 0;
    virtual bool HitTest(FVector2 P) const = 0;
    virtual void OnMouseButtonDown(int32 X, int32 Y);
    virtual void OnMouseButtonUp(int32 X, int32 Y);
    virtual void OnMouseMove(float DeltaX, float DeltaY);

    virtual ~SWidget();

  protected:
    SWidget() = default;
    bool bIsClicked = false;
};
