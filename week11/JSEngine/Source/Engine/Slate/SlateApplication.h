#pragma once
#include "Runtime/WindowMessageHandler.h"
#include "Core/Singleton.h"

class SWidget;
class SWindow;

/*
* Slate 총괄 및 입력 처리를 담당하는 싱글턴.
* 위젯 트리의 루트 윈도우만 알고 있으며,
* 트리 내부 구조(Splitter, Viewport 등)는 소유하지 않습니다.
*/
class FSlateApplication : public TSingleton<FSlateApplication>, public IWindowMessageHandler
{
	friend class TSingleton<FSlateApplication>;
public:
	void Initialize();
	void Shutdown();

	void Tick(float DeltaTime);
	void Paint();

	bool OnMouseMove(void* hwnd, int32 X, int32 Y) override;
	bool OnMouseButtonDown(void* hwnd, int32 Button, int32 X, int32 Y) override;
	bool OnMouseButtonUp(void* hwnd, int32 Button, int32 X, int32 Y) override;
	bool OnMouseWheel(void* hwnd, int32 Delta, int32 X, int32 Y) override;
	bool OnKeyDown(void* hwnd, uint32 Key) override;
	bool OnKeyUp(void* hwnd, uint32 Key) override;
	bool OnChar(void* hwnd, uint32 Codepoint) override;
	bool OnResize(void* hwnd, int32 Width, int32 Height) override;
	bool OnSetFocus(void* hwnd) override;
	bool OnKillFocus(void* hwnd) override;

	// Get Set
	SWindow* GetRootWindow() const { return RootWindow; }

	SWidget* GetFocusedWidget() const { return FocusedWidget; }
	void SetFocusedWidget(SWidget* InWidget) { FocusedWidget = InWidget; }

	SWidget* GetHoveredWidget() const { return HoveredWidget; }
	void SetHoveredWidget(SWidget* InWidget) { HoveredWidget = InWidget; }

	SWidget* GetCapturedWidget() const { return CapturedWidget; }
	void SetCapturedWidget(SWidget* InWidget) { CapturedWidget = InWidget; }

	// 위젯 트리가 파괴되기 전에 모든 위젯 참조를 일괄 해제
	void ClearWidgetRefs();

private:
	SWidget* HitTest(int32 X, int32 Y);

private:
	SWindow* RootWindow = nullptr;
	SWidget* FocusedWidget = nullptr;
	SWidget* HoveredWidget = nullptr;
	SWidget* CapturedWidget = nullptr;
};
