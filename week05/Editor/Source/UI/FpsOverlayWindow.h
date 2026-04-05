#pragma once
#include "imgui.h"

struct FRect;
class FEditorEngine;

class FFpsOverlayWindow
{
public:
    void Render(FEditorEngine* Engine, const FRect& AreaRect);
};
