#pragma once

#include <cmath>

#include <windows.h>

#include <Xinput.h>

class UInputManager
{
public:
	UInputManager() :
		bIsPrevMouseDown(false),
		bIsMouseDown(false),
		bIsMousePressed(false),
		bIsMouseReleased(false),
		bIsXPadConnected(false),
		MousePosition(0, 0),
		XPadStickX(0),
		XPadStickY(0),
		bIsXPadADown(false),
		bIsXPadTriggerDown(false)
	{
	}

	void Update(HWND hWnd, float dt);

	bool IsMousePressed() const { return bIsMousePressed; }
	bool IsMouseReleased() const { return bIsMouseReleased; }
	bool IsMouseDown() const { return bIsMouseDown; }

	bool IsXPadConnected() const { return bIsXPadConnected; }
	POINT GetMousePosition() const { return MousePosition; }

	float GetXPadStickX() const { return XPadStickX; }
	float GetXPadStickY() const { return XPadStickY; }

	bool IsXPadADown() const { return bIsXPadADown; }
	bool IsXPadAPressed() const { return bIsXPadAPressed; }
	bool IsXPadAReleased() const { return bIsXPadAReleased; }

	bool IsXPadTriggerDown() const { return bIsXPadTriggerDown; }
	bool IsXPadHoldingRepeatTriggered() const { return bIsHoldingRepeatTriggered; }

private:
	bool bIsPrevMouseDown = false;
	bool bIsMouseDown = false;
	bool bIsMousePressed = false;
	bool bIsMouseReleased = false;

	bool bIsPrevXPadADown = false;
	bool bIsXPadADown = false;
	bool bIsXPadAPressed = false;
	bool bIsXPadAReleased = false;

	bool bIsXPadConnected;
	POINT MousePosition;

	// [-1, 1] Normalized
	float XPadStickX;
	float XPadStickY;

	bool bIsXPadTriggerDown;

	bool bIsHoldingRepeatStarted;
	bool bIsHoldingRepeatTriggered;
	float HoldRepeatTime;
};
