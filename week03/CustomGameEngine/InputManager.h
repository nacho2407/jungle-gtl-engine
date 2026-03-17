#pragma once

#include "Singleton.h"
#include "Math/Vector.h"
#include <Windows.h>

class InputManager : public ISingleton<InputManager>
{
	friend class ISingleton<InputManager>;
private:
	InputManager();
public:
	~InputManager();

	void Update();
	void UpdateKeyState();
	void ClearKeyState();

	bool IsKeyDown(int key);
	bool IsKeyHold(int key);
	bool IsKeyUp(int key);

	void UpdateMouse();

public:
	bool CurrentState[256];
	bool PrevState[256];
	//bool bIsActive{ true };
	bool bJustGainedFocus{ true };

	FVector MousePos;
	FVector LastMousePos;
	FVector MouseDelta;

	HWND hWnd;

private:

};
