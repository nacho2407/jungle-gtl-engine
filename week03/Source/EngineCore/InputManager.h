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

	// 메시지 처리용
	void AccumulateMouseWheelDelta(float delta);

public:
	bool CurrentState[256];
	bool PrevState[256];
	//bool bIsActive{ true };
	bool bJustGainedFocus{ true };

	FVector MousePos;
	FVector LastMousePos;
	FVector MouseDelta;
	float MouseWheel = 0.0f; // 현재 프레임에서 확정된 마우스 휠 변화량

	HWND hWnd;

private:
	float MouseWheelAccumulator = 0.0f;
};
