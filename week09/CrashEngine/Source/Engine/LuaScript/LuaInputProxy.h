#pragma once

#include "Core/CoreTypes.h"

class FLuaInputProxy
{
public:
    bool IsKeyDown(const FString& KeyName) const;
    bool WasKeyPressed(const FString& KeyName) const;
    bool WasKeyReleased(const FString& KeyName) const;

    bool IsMouseButtonDown(const FString& ButtonName) const;
    bool WasMouseButtonPressed(const FString& ButtonName) const;
    bool WasMouseButtonReleased(const FString& ButtonName) const;

    bool IsGamepadConnected(int32 ControllerId = 0) const;
    bool IsGamepadButtonDown(const FString& ButtonName, int32 ControllerId = 0) const;
    bool WasGamepadButtonPressed(const FString& ButtonName, int32 ControllerId = 0) const;
    bool WasGamepadButtonReleased(const FString& ButtonName, int32 ControllerId = 0) const;

    float GetAxis(const FString& AxisName, int32 ControllerId = 0) const;
};

/*
 * Helper Functions
 */

FString LuaKeyNameFromVK(int32 VK);
FString LuaGamepadButtonNameFromIndex(int32 ButtonIndex);
