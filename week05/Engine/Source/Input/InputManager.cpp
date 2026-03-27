#include "InputManager.h"
#include <cstring>

void CInputManager::ProcessMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	switch (Msg)
	{
	case WM_KEYDOWN:
		if (WParam < MAX_KEYS)
			EventQueue.push_back({ EInputEventType::KeyDown, static_cast<int32>(WParam) });
		break;

	case WM_KEYUP:
		if (WParam < MAX_KEYS)
			EventQueue.push_back({ EInputEventType::KeyUp, static_cast<int32>(WParam) });
		break;

	case WM_LBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_LEFT });
		SetCapture(Hwnd);
		break;

	case WM_LBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_LEFT });
		ReleaseCapture();
		break;

	case WM_RBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_RIGHT });
		GetCursorPos(&LastMousePos);
		bTrackingMouse = true;
		SetCapture(Hwnd);
		break;

	case WM_RBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_RIGHT });
		bTrackingMouse = false;
		ReleaseCapture();
		break;

	case WM_MBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_MIDDLE });
		break;

	case WM_MBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_MIDDLE });
		break;
	}
}

void CInputManager::Tick()
{
	// Save previous frame state
	std::memcpy(PrevKeyState, KeyState, sizeof(KeyState));
	std::memcpy(PrevMouseButtonState, MouseButtonState, sizeof(MouseButtonState));

	// Flush event queue
	for (const FInputEvent& Event : EventQueue)
	{
		switch (Event.Type)
		{
		case EInputEventType::KeyDown:
			KeyState[Event.KeyOrButton] = true;
			break;
		case EInputEventType::KeyUp:
			KeyState[Event.KeyOrButton] = false;
			break;
		case EInputEventType::MouseButtonDown:
			MouseButtonState[Event.KeyOrButton] = true;
			break;
		case EInputEventType::MouseButtonUp:
			MouseButtonState[Event.KeyOrButton] = false;
			break;
		}
	}
	EventQueue.clear();

	// Mouse delta
	if (bTrackingMouse)
	{
		POINT CurrentPos;
		GetCursorPos(&CurrentPos);
		MouseDeltaX = static_cast<float>(CurrentPos.x - LastMousePos.x);
		MouseDeltaY = static_cast<float>(CurrentPos.y - LastMousePos.y);
		LastMousePos = CurrentPos;
	}
	else
	{
		MouseDeltaX = 0.0f;
		MouseDeltaY = 0.0f;
	}
}

bool CInputManager::IsKeyDown(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return KeyState[Key];
}

bool CInputManager::IsKeyPressed(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return KeyState[Key] && !PrevKeyState[Key];
}

bool CInputManager::IsKeyReleased(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return !KeyState[Key] && PrevKeyState[Key];
}

bool CInputManager::IsMouseButtonDown(int32 Button) const
{
	if (Button < 0 || Button >= MAX_MOUSE_BUTTONS) return false;
	return MouseButtonState[Button];
}

bool CInputManager::IsMouseButtonPressed(int32 Button) const
{
	if (Button < 0 || Button >= MAX_MOUSE_BUTTONS) return false;
	return MouseButtonState[Button] && !PrevMouseButtonState[Button];
}

bool CInputManager::IsMouseButtonReleased(int32 Button) const
{
	if (Button < 0 || Button >= MAX_MOUSE_BUTTONS) return false;
	return !MouseButtonState[Button] && PrevMouseButtonState[Button];
}
