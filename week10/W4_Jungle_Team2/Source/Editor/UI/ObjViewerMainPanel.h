#pragma once

#include "ObjViewerMenuBarWidget.h"
#include "ObjViewerControlWidget.h"
#include "ObjViewerStatWidget.h"

class UObjViewerEngine;
class FWindowsWindow;
class FRenderer;

class FObjViewerMainPanel
{
public:
    void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine);
    void Render(float DeltaTime);
    void Shutdown();

private:
    FWindowsWindow* Window = nullptr;

    // 하위 위젯 인스턴스
    FObjViewerMenuBarWidget MenuBarWidget;
    FObjViewerControlWidget ControlWidget;
    FObjViewerStatWidget    StatWidget;
};