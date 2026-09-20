#include "UInputManager.hpp"

void UInputManager::Update(HWND hWnd, float dt)
{
	bool bCurrent = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	bIsMousePressed = (!bIsPrevMouseDown && bCurrent);
	bIsMouseReleased = (bIsPrevMouseDown && !bCurrent);
	bIsMouseDown = bCurrent;
	bIsPrevMouseDown = bCurrent;

	GetCursorPos(&MousePosition);
	ScreenToClient(hWnd, &MousePosition);

	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));

	DWORD result = XInputGetState(0, &state);
	bIsXPadConnected = (result == ERROR_SUCCESS);

	if (bIsXPadConnected)
	{
		// 패드 연결됨

		// [-32768, 32767]
		SHORT lx = state.Gamepad.sThumbLX;
		SHORT ly = state.Gamepad.sThumbLY;

		float Magnitude = sqrt(lx * lx + ly * ly);
		const float DEADZONE = 8689;

		// HW Noise 배제
		if (Magnitude < DEADZONE)
		{
			// Not Holding
			lx = 0;
			ly = 0;

			if (bIsHoldingRepeatStarted)
			{
				bIsHoldingRepeatStarted = false;
				bIsHoldingRepeatTriggered = false;
				HoldRepeatTime = 0;
			}
		}
		else
		{
			// Holding
			if (bIsHoldingRepeatStarted)
			{
				HoldRepeatTime += dt;
				if (HoldRepeatTime >= 0.3f)
				{
					bIsHoldingRepeatTriggered = true;
					// dt 가 큰 경우 0.3f 를 크게 벗어날 수 있으므로 이를 고려해서 HoldRepeatTime 수정
					HoldRepeatTime -= 0.3f;
				}
				else
				{
					bIsHoldingRepeatTriggered = false;
				}
			}
			else
			{
				// 첫 시작시엔 바로 Trigger
				bIsHoldingRepeatStarted = true;
				bIsHoldingRepeatTriggered = true;
				HoldRepeatTime = 0;
			}
		}


		// lx, ly => [-1, 1] 정규화
		XPadStickX = lx / 32768.f;
		XPadStickY = ly / 32768.f;

		bIsXPadADown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
		bIsXPadAPressed = bIsXPadADown && !bIsPrevXPadADown;
		bIsXPadAReleased = !bIsXPadADown && bIsPrevXPadADown;
		bIsPrevXPadADown = bIsXPadADown;

		bIsXPadTriggerDown = (state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD || state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
	}
}
