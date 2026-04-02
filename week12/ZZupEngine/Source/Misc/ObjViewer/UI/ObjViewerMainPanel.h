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
	// TODO-VIEWER: Slate 구조 개편 후 엔진 포인터 삭제하기
	UObjViewerEngine* Engine = nullptr;
	FWindowsWindow* Window = nullptr;

	// 하위 위젯 인스턴스
	FObjViewerMenuBarWidget MenuBarWidget;
	FObjViewerControlWidget ControlWidget;
	FObjViewerStatWidget    StatWidget;
};