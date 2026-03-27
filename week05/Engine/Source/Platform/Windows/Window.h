#pragma once
#include "CoreMinimal.h"
#include <Windows.h>
#include <functional>
#include "WindowTypes.h"

class ENGINE_API CWindow
{
public:
	CWindow() = default;
	~CWindow();

	bool Create(HINSTANCE Instance, const WCHAR* ClassName,
		const WCHAR* Title, int InWidth, int InHeight, int InX, int InY);
	void Destroy();

	void Show();
	void Hide();

	HWND GetHwnd() const { return Hwnd; }
	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }

	void AddMessageFilter(FWndProcFilter Filter);
	void SetOnResizeCallback(FOnResizeCallback Callback);

	LRESULT HandleMessage(UINT Msg, WPARAM wParam, LPARAM lParam);

private:
	HWND Hwnd = nullptr;
	int Width = 0;
	int Height = 0;
	int PosX = 0;
	int PosY = 0;

	TArray<FWndProcFilter> MessageFilters;
	FOnResizeCallback OnResizeCallback;
};
