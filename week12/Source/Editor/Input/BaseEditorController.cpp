#include "BaseEditorController.h"

void IBaseEditorController::SetViewportDim(float X, float Y, float Width, float Height)
{
    ViewportX = X;
    ViewportY = Y;
    ViewportWidth = Width;
    ViewportHeight = Height;
}