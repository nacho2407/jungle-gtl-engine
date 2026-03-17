#include "InputManager.h"

InputManager::InputManager() {
	for (int i = 0; i < 256; ++i) {
		PrevState[i] = false;
		CurrentState[i] = false;
	}
}

InputManager::~InputManager() {}

void InputManager::Update()
{
	if (hWnd)
	{
		GetInstance().UpdateKeyState();
		GetInstance().UpdateMouse();
	}
	else
	{
		GetInstance().ClearKeyState();
		bJustGainedFocus = true;
	}
}

void InputManager::UpdateKeyState() {
	for (int i = 0; i < 256; ++i) {
		PrevState[i] = CurrentState[i];
		CurrentState[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
	}
}

void InputManager::ClearKeyState() {
	for (int i = 0; i < 256; ++i) {
		PrevState[i] = false;
		CurrentState[i] = false;
	}
}

bool InputManager::IsKeyDown(int vk) {
	return CurrentState[vk] && !PrevState[vk];
}

bool InputManager::IsKeyUp(int vk) {
	return !CurrentState[vk] && PrevState[vk];
}

bool InputManager::IsKeyHold(int vk) {
	return CurrentState[vk];
}

void InputManager::UpdateMouse()
{
	POINT pt;
	GetCursorPos(&pt);

	ScreenToClient(hWnd, &pt);

	MousePos.x = static_cast<float>(pt.x);
	MousePos.y = static_cast<float>(pt.y);

	if (bJustGainedFocus)
	{
		MouseDelta.x = 0.0f;
		MouseDelta.y = 0.0f;
		LastMousePos = MousePos;
		bJustGainedFocus = false;
		return;
	}

	MouseDelta.x = MousePos.x - LastMousePos.x;
	MouseDelta.y = MousePos.y - LastMousePos.y;
	LastMousePos = MousePos;
}
