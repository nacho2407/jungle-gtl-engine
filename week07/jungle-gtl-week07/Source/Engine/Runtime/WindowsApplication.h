#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>

#include "Engine/Runtime/WindowsWindow.h"

using FOnSizingCallback = std::function<void()>;
using FOnResizedCallback = std::function<void(unsigned int, unsigned int)>;

class FWindowsApplication
{
public:
	FWindowsApplication() = default;
	~FWindowsApplication() = default;

	bool Init(HINSTANCE InHInstance);
	void PumpMessages();
	void Destroy();

	FWindowsWindow& GetWindow() { return Window; }
	const FWindowsWindow& GetWindow() const { return Window; }

	bool IsExitRequested() const { return bIsExitRequested; }
	bool IsResizing() const { return bIsResizing; }

	void SetOnSizingCallback(FOnSizingCallback InCallback) { OnSizingCallback = std::move(InCallback); }
	void SetOnResizedCallback(FOnResizedCallback InCallback) { OnResizedCallback = std::move(InCallback); }

private:
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

private:
	HINSTANCE HInstance = nullptr;
	FWindowsWindow Window;

	bool bIsExitRequested = false;
	bool bIsResizing = false;

	FOnSizingCallback OnSizingCallback;
	FOnResizedCallback OnResizedCallback;
};
