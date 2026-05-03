#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Math/Vector.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
    FString NormalizeInputName(FString Name)
    {
        Name.erase(
            std::remove_if(
                Name.begin(),
                Name.end(),
                [](unsigned char Ch)
                {
                    return Ch == ' ' || Ch == '_' || Ch == '-';
                }),
            Name.end());

        std::transform(
            Name.begin(),
            Name.end(),
            Name.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Name;
    }

    int32 ResolveInputKeyCode(const FString& KeyName)
    {
        if (KeyName.empty())
        {
            return -1;
        }

        if (KeyName.size() == 1)
        {
            const unsigned char Ch = static_cast<unsigned char>(KeyName[0]);
            if (std::isalnum(Ch))
            {
                return static_cast<int32>(std::toupper(Ch));
            }
        }

        const FString Key = NormalizeInputName(KeyName);
        if (Key.empty())
        {
            return -1;
        }

        if (Key.size() == 1)
        {
            const unsigned char Ch = static_cast<unsigned char>(Key[0]);
            if (std::isalnum(Ch))
            {
                return static_cast<int32>(std::toupper(Ch));
            }
        }

        if (Key == "leftmouse" || Key == "mouseleft" || Key == "lmb") return VK_LBUTTON;
        if (Key == "rightmouse" || Key == "mouseright" || Key == "rmb") return VK_RBUTTON;
        if (Key == "middlemouse" || Key == "mousemiddle" || Key == "mmb") return VK_MBUTTON;
        if (Key == "xbutton1" || Key == "mousex1") return VK_XBUTTON1;
        if (Key == "xbutton2" || Key == "mousex2") return VK_XBUTTON2;

        if (Key == "space" || Key == "spacebar") return VK_SPACE;
        if (Key == "escape" || Key == "esc") return VK_ESCAPE;
        if (Key == "enter" || Key == "return") return VK_RETURN;
        if (Key == "tab") return VK_TAB;
        if (Key == "backspace") return VK_BACK;
        if (Key == "delete" || Key == "del") return VK_DELETE;
        if (Key == "insert" || Key == "ins") return VK_INSERT;
        if (Key == "home") return VK_HOME;
        if (Key == "end") return VK_END;
        if (Key == "pageup") return VK_PRIOR;
        if (Key == "pagedown") return VK_NEXT;

        if (Key == "shift") return VK_SHIFT;
        if (Key == "leftshift") return VK_LSHIFT;
        if (Key == "rightshift") return VK_RSHIFT;
        if (Key == "ctrl" || Key == "control") return VK_CONTROL;
        if (Key == "leftctrl" || Key == "leftcontrol") return VK_LCONTROL;
        if (Key == "rightctrl" || Key == "rightcontrol") return VK_RCONTROL;
        if (Key == "alt" || Key == "menu") return VK_MENU;
        if (Key == "leftalt" || Key == "leftmenu") return VK_LMENU;
        if (Key == "rightalt" || Key == "rightmenu") return VK_RMENU;

        if (Key == "left" || Key == "leftarrow") return VK_LEFT;
        if (Key == "right" || Key == "rightarrow") return VK_RIGHT;
        if (Key == "up" || Key == "uparrow") return VK_UP;
        if (Key == "down" || Key == "downarrow") return VK_DOWN;

        if (Key.size() >= 2 && Key[0] == 'f')
        {
            const FString NumberPart = Key.substr(1);
            if (!NumberPart.empty() && std::all_of(NumberPart.begin(), NumberPart.end(), [](unsigned char Ch) { return std::isdigit(Ch); }))
            {
                const int32 FunctionIndex = std::atoi(NumberPart.c_str());
                if (FunctionIndex >= 1 && FunctionIndex <= 24)
                {
                    return VK_F1 + FunctionIndex - 1;
                }
            }
        }

        return -1;
    }

    bool IsValidInputKeyCode(int32 KeyCode)
    {
        return KeyCode >= 0 && KeyCode < 256;
    }

    bool IsMouseButtonCode(int32 KeyCode)
    {
        return KeyCode == VK_LBUTTON
            || KeyCode == VK_RBUTTON
            || KeyCode == VK_MBUTTON
            || KeyCode == VK_XBUTTON1
            || KeyCode == VK_XBUTTON2;
    }

    bool CanExposeInputKeyToLua(int32 KeyCode)
    {
        if (!IsValidInputKeyCode(KeyCode))
        {
            return false;
        }

        if (GEngine
            && GEngine->GetRuntimeInputMode() == ERuntimeInputMode::GameOnly
            && GEngine->IsRuntimeCursorLocked())
        {
            return true;
        }

        const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
        if (IsMouseButtonCode(KeyCode))
        {
            return !(GuiState.bUsingMouse || GuiState.bBlockViewportMouse);
        }

        return !(GuiState.bUsingKeyboard || GuiState.bUsingTextInput);
    }

    bool CanExposeMouseAxisToLua()
    {
        if (GEngine
            && GEngine->GetRuntimeInputMode() == ERuntimeInputMode::GameOnly
            && GEngine->IsRuntimeCursorLocked())
        {
            return true;
        }

        const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
        return !(GuiState.bUsingMouse || GuiState.bBlockViewportMouse);
    }

    ERuntimeInputMode ParseRuntimeInputMode(const FString& Mode)
    {
        const FString Normalized = NormalizeInputName(Mode);
        if (Normalized == "uionly" || Normalized == "ui")
        {
            return ERuntimeInputMode::UIOnly;
        }
        if (Normalized == "gameandui" || Normalized == "gameui" || Normalized == "both")
        {
            return ERuntimeInputMode::GameAndUI;
        }
        return ERuntimeInputMode::GameOnly;
    }
}

namespace FLuaEngineAPI
{
    void BindInput(sol::state& Lua, sol::table& API)
    {
        sol::table Input = Lua.create_table();

        Input["IsKeyDown"] = sol::overload(
            [](const FString& KeyName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(KeyName);
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKey(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKey(KeyCode);
            });

        Input["IsKeyPressed"] = sol::overload(
            [](const FString& KeyName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(KeyName);
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKeyDown(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKeyDown(KeyCode);
            });

        Input["IsKeyReleased"] = sol::overload(
            [](const FString& KeyName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(KeyName);
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKeyUp(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && InputSystem::Get().GetKeyUp(KeyCode);
            });

        Input["IsMouseDown"] = sol::overload(
            [](const FString& ButtonName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(ButtonName);
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKey(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKey(KeyCode);
            });

        Input["IsMousePressed"] = sol::overload(
            [](const FString& ButtonName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(ButtonName);
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKeyDown(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKeyDown(KeyCode);
            });

        Input["IsMouseReleased"] = sol::overload(
            [](const FString& ButtonName) -> bool
            {
                const int32 KeyCode = ResolveInputKeyCode(ButtonName);
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKeyUp(KeyCode);
            },
            [](int32 KeyCode) -> bool
            {
                return CanExposeInputKeyToLua(KeyCode) && IsMouseButtonCode(KeyCode) && InputSystem::Get().GetKeyUp(KeyCode);
            });

        Input["GetMousePosition"] = []() -> FVector
        {
            const POINT MousePos = InputSystem::Get().GetMousePos();
            return FVector(static_cast<float>(MousePos.x), static_cast<float>(MousePos.y), 0.0f);
        };

        Input["GetMouseDelta"] = []() -> FVector
        {
            if (!CanExposeMouseAxisToLua())
            {
                return FVector::ZeroVector;
            }
            return FVector(
                static_cast<float>(InputSystem::Get().MouseDeltaX()),
                static_cast<float>(InputSystem::Get().MouseDeltaY()),
                0.0f);
        };

        Input["GetScrollDelta"] = []() -> int32
        {
            if (!CanExposeMouseAxisToLua())
            {
                return 0;
            }
            return InputSystem::Get().GetScrollDelta();
        };

        Input["GetScrollNotches"] = []() -> float
        {
            if (!CanExposeMouseAxisToLua())
            {
                return 0.0f;
            }
            return InputSystem::Get().GetScrollNotches();
        };

        Input["IsAnyMouseButtonDown"] = []() -> bool
        {
            if (!CanExposeMouseAxisToLua())
            {
                return false;
            }
            return InputSystem::Get().IsAnyMouseButtonDown();
        };

        Input["SetInputMode"] = [](const FString& Mode)
        {
            if (GEngine)
            {
                const ERuntimeInputMode ParsedMode = ParseRuntimeInputMode(Mode);
                GEngine->SetRuntimeInputMode(ParsedMode);
                GEngine->SetRuntimeCursorVisible(ParsedMode != ERuntimeInputMode::GameOnly);
            }
        };

        Input["SetInputModeGameOnly"] = []()
        {
            if (GEngine)
            {
                GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameOnly);
                GEngine->SetRuntimeCursorVisible(false);
            }
        };

        Input["SetInputModeUIOnly"] = []()
        {
            if (GEngine)
            {
                GEngine->SetRuntimeInputMode(ERuntimeInputMode::UIOnly);
                GEngine->SetRuntimeCursorVisible(true);
            }
        };

        Input["SetInputModeGameAndUI"] = []()
        {
            if (GEngine)
            {
                GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameAndUI);
                GEngine->SetRuntimeCursorVisible(true);
            }
        };

        Input["SetCursorVisible"] = [](bool bVisible)
        {
            if (GEngine)
            {
                GEngine->SetRuntimeCursorVisible(bVisible);
            }
            InputSystem::Get().SetCursorVisibility(bVisible);
        };

        Input["SetCursorLocked"] = [](bool bLocked)
        {
            if (GEngine)
            {
                GEngine->SetRuntimeCursorLocked(bLocked);
            }
            if (!bLocked)
            {
                InputSystem::Get().SetUseRawMouse(false);
                InputSystem::Get().LockMouse(false);
            }
        };

        Input["SetMouseCapture"] = [](bool bCaptured)
        {
            if (GEngine)
            {
                GEngine->SetRuntimeInputMode(bCaptured ? ERuntimeInputMode::GameOnly : ERuntimeInputMode::GameAndUI);
            }
        };

        Input["ReleaseMouseCapture"] = []()
        {
            if (GEngine)
            {
                GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameAndUI);
            }
        };

        Input["IsMouseCaptured"] = []() -> bool
        {
            return GEngine ? GEngine->IsRuntimeCursorLocked() && !GEngine->IsRuntimeCursorVisible() : false;
        };

        Input["IsCursorLocked"] = []() -> bool
        {
            return GEngine ? GEngine->IsRuntimeCursorLocked() : false;
        };

        Input["IsCursorVisible"] = []() -> bool
        {
            return GEngine ? GEngine->IsRuntimeCursorVisible() : false;
        };

        API["Input"] = Input;
    }
}
