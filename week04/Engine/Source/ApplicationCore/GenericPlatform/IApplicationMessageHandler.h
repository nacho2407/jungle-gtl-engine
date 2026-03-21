#pragma once
#include <Core/CoreMinimal.h>
#include "ApplicationCore/Input/InputEvent.h"

class IApplicationMessageHandler
{
  public:
    virtual ~IApplicationMessageHandler() = default;

    virtual bool OnKeyDown(EKey Key, bool bIsRepeat) = 0;
    virtual bool OnKeyUp(EKey Key) = 0;

    virtual bool OnMouseDown(EKey Button, int32 X, int32 Y) = 0;
    virtual bool OnMouseUp(EKey Button, int32 X, int32 Y) = 0;
    virtual bool OnMouseDoubleClick(EKey Button, int32 X, int32 Y) = 0;

    virtual bool OnMouseMove(int32 X, int32 Y) = 0;
    virtual bool OnRawMouseMove(int32 DeltaX, int32 DeltaY) = 0;
    virtual bool OnMouseWheel(float Delta, int32 X, int32 Y) = 0;

    virtual bool OnSizeChanged(int32 Width, int32 Height) = 0;
    virtual void OnFocusLost() = 0;
};