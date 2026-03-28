#pragma once
#include "Runtime/WindowMessageHandler.h"
#include "Core/Singleton.h"

class SWidget;
/*
* Slate 총괄 및 입력 처리를 담당하는 짬통
*/
class FSlateApplication : public TSingleton<FSlateApplication>, public IWindowMessageHandler
{
	friend class TSingleton<FSlateApplication>;
public:
	void Initialize();
	void Shutdown();

	void SetRootWindow(class SWindow* InRootWindow);
	class SWindow* GetRootWindow() const;

	void Tick(float DeltaTime);
	void Paint();

	bool OnMouseMove(void* hwnd, int X, int Y) override;
	bool OnMouseButtonDown(void* hwnd, int Button, int X, int Y) override;
	bool OnMouseButtonUp(void* hwnd, int Button, int X, int Y) override;
	bool OnMouseWheel(void* hwnd, int Delta, int X, int Y) override;
	bool OnKeyDown(void* hwnd, uint32 Key) override;
	bool OnKeyUp(void* hwnd, uint32 Key) override;
	bool OnChar(void* hwnd, uint32 Codepoint) override;
	bool OnResize(void* hwnd, int Width, int Height) override;
	bool OnSetFocus(void* hwnd) override;
	bool OnKillFocus(void* hwnd) override;

private:
	SWidget* HitTest(int X, int Y);

private:
	class SWindow* RootWindow = nullptr;
	class SWidget* FocusedWidget = nullptr;
	class SWidget* HoveredWidget = nullptr;
	class SWidget* CapturedWidget = nullptr;
};

