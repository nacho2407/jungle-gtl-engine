#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

#include "Engine/Core/CoreTypes.h"

struct FWindowHitTestRect
{
	int32 Left = 0;
	int32 Top = 0;
	int32 Right = 0;
	int32 Bottom = 0;

	bool Contains(int32 X, int32 Y) const
	{
		return X >= Left && X < Right && Y >= Top && Y < Bottom;
	}
};

struct FCustomTitleBarState
{
	int32 TitleBarHeight = 0;
	std::vector<FWindowHitTestRect> InteractiveRects;
};

class FWindowsWindow
{
public:
	FWindowsWindow() = default;
	~FWindowsWindow() = default;

	void Initialize(HWND InHWindow, const wchar_t* InTitle = L"");

	HWND GetHWND() const { return HWindow; }
	const std::wstring& GetTitle() const { return Title; }

	float GetWidth() const { return Width; }
	float GetHeight() const { return Height; }

	void OnResized(unsigned int InWidth, unsigned int InHeight);

	POINT ScreenToClientPoint(POINT ScreenPoint) const;

	void SetCustomTitleBarMetrics(int32 Height, const std::vector<FWindowHitTestRect>& InteractiveRects);
	const FCustomTitleBarState& GetCustomTitleBarState() const { return CustomTitleBarState; }

	void Minimize();
	void ToggleMaximize();
	void Close();
	bool IsWindowMaximized() const;

private:
	HWND HWindow = nullptr;
	std::wstring Title;
	float Width = 0.f;
	float Height = 0.f;
	FCustomTitleBarState CustomTitleBarState;
};
