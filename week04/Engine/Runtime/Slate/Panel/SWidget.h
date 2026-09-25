#pragma once

class SWidget
{
  public:
    //virtual void Render(ID3D11DeviceContext* Context) = 0;
    virtual bool HitTest(int32 X, int32 Y) const = 0;
    virtual void OnMouseButtonDown(int32 X, int32 Y) {}
    virtual void OnMouseButtonUp(int32 X, int32 Y) {}
    virtual void OnMouseMove(float DeltaX, float DeltaY) {}

    virtual ~SWidget() = default;
};