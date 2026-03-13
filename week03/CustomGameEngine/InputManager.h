#pragma once

#include <Windows.h>
#include "ISingleton.h"

class InputManager : public ISingleton<InputManager>
{
	friend class ISingleton<InputManager>;
private:
	InputManager();
public:
	~InputManager();

	void Update();

	bool IsKeyDown(int key);
	bool IsKeyHold(int key);
	bool IsKeyUp(int key);

public:
	bool CurrentState[256];
	bool PrevState[256];

private:

};
