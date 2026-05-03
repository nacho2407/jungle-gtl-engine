#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Runtime/Engine.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
    FString ToPx(float Value)
    {
        return std::to_string(Value) + "px";
    }

    FString ToCssColor(float R, float G, float B, float A)
    {
        const int32 RI = static_cast<int32>(std::clamp(R, 0.0f, 1.0f) * 255.0f);
        const int32 GI = static_cast<int32>(std::clamp(G, 0.0f, 1.0f) * 255.0f);
        const int32 BI = static_cast<int32>(std::clamp(B, 0.0f, 1.0f) * 255.0f);
        const float Alpha = std::clamp(A, 0.0f, 1.0f);

        char Buffer[96] = {};
        std::snprintf(Buffer, sizeof(Buffer), "rgba(%d,%d,%d,%.3f)", RI, GI, BI, Alpha);
        return FString(Buffer);
    }

    bool SetStyle(const FString& ElementId, const FString& Name, const FString& Value)
    {
        return GEngine ? GEngine->SetRmlUIElementStyle(ElementId, Name, Value) : false;
    }

    bool SetAttribute(const FString& ElementId, const FString& Name, const FString& Value)
    {
        return GEngine ? GEngine->SetRmlUIElementAttribute(ElementId, Name, Value) : false;
    }

    bool SetTransformStyles(const FString& ElementId, float X, float Y, float W, float H)
    {
        if (!GEngine)
        {
            return false;
        }

        bool bResult = GEngine->SetRmlUIElementStyle(ElementId, "position", "absolute");
        bResult = GEngine->SetRmlUIElementStyle(ElementId, "left", ToPx(X)) || bResult;
        bResult = GEngine->SetRmlUIElementStyle(ElementId, "top", ToPx(Y)) || bResult;
        bResult = GEngine->SetRmlUIElementStyle(ElementId, "width", ToPx(W)) || bResult;
        bResult = GEngine->SetRmlUIElementStyle(ElementId, "height", ToPx(H)) || bResult;
        return bResult;
    }

    bool UnsupportedLegacyCreate(sol::variadic_args)
    {
        // RmlUi 전환 후 위젯 생성은 Lua 런타임 함수가 아니라 .rml 문서가 담당합니다.
        return false;
    }

    bool UnsupportedLegacyOption(sol::variadic_args)
    {
        return false;
    }
}

namespace FLuaEngineAPI
{
    void BindUI(sol::state& Lua, sol::table& API)
    {
        sol::table UI = Lua.create_table();

        UI["LoadDocument"] = [](const FString& ScreenId, const FString& Path) -> bool
        {
            return GEngine ? GEngine->LoadRmlUIDocument(ScreenId, Path) : false;
        };

        UI["UnloadDocument"] = [](const FString& ScreenId) -> bool
        {
            return GEngine ? GEngine->UnloadRmlUIDocument(ScreenId) : false;
        };

        UI["ReloadDocument"] = [](const FString& ScreenId) -> bool
        {
            return GEngine ? GEngine->ReloadRmlUIDocument(ScreenId) : false;
        };

        UI["ShowDocument"] = [](const FString& ScreenId) -> bool
        {
            return GEngine ? GEngine->ShowRmlUIScreen(ScreenId) : false;
        };

        UI["HideDocument"] = [](const FString& ScreenId) -> bool
        {
            return GEngine ? GEngine->HideRmlUIScreen(ScreenId) : false;
        };

        UI["ShowScreen"] = sol::overload(
            [](const FString& ScreenId) -> bool
            {
                return GEngine ? GEngine->ShowRmlUIScreen(ScreenId) : false;
            },
            [](const FString& ScreenId, const FString&) -> bool
            {
                return GEngine ? GEngine->ShowRmlUIScreen(ScreenId) : false;
            });

        UI["SetElementText"] = [](const FString& ElementId, const FString& Text) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementText(ElementId, Text) : false;
        };

        UI["SetElementVisible"] = [](const FString& ElementId, bool bVisible) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementVisible(ElementId, bVisible) : false;
        };

        UI["SetElementEnabled"] = [](const FString& ElementId, bool bEnabled) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementEnabled(ElementId, bEnabled) : false;
        };

        UI["SetElementClass"] = [](const FString& ElementId, const FString& ClassName, bool bEnabled) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementClass(ElementId, ClassName, bEnabled) : false;
        };

        UI["SetElementAttribute"] = [](const FString& ElementId, const FString& Name, const FString& Value) -> bool
        {
            return SetAttribute(ElementId, Name, Value);
        };

        UI["SetElementStyle"] = [](const FString& ElementId, const FString& Name, const FString& Value) -> bool
        {
            return SetStyle(ElementId, Name, Value);
        };

        UI["SetText"] = [](const FString& WidgetId, const FString& Text) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementText(WidgetId, Text) : false;
        };

        UI["SetImage"] = [](const FString& WidgetId, const FString& ImagePath) -> bool
        {
            return SetAttribute(WidgetId, "src", ImagePath);
        };

        UI["SetProgress"] = [](const FString& WidgetId, float Value) -> bool
        {
            return SetAttribute(WidgetId, "value", std::to_string(Value));
        };

        UI["SetVisible"] = [](const FString& WidgetId, bool bVisible) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementVisible(WidgetId, bVisible) : false;
        };

        UI["SetEnabled"] = [](const FString& WidgetId, bool bEnabled) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementEnabled(WidgetId, bEnabled) : false;
        };

        UI["SetActionEvent"] = [](const FString& WidgetId, const FString& EventName) -> bool
        {
            return SetAttribute(WidgetId, "data-action", EventName);
        };

        UI["RemoveWidget"] = [](const FString& WidgetId) -> bool
        {
            bool bResult = GEngine ? GEngine->SetRmlUIElementVisible(WidgetId, false) : false;
            bResult = SetAttribute(WidgetId, "disabled", "true") || bResult;
            return bResult;
        };

        UI["SetZOrder"] = [](const FString& WidgetId, int32 ZOrder) -> bool
        {
            return SetStyle(WidgetId, "z-index", std::to_string(ZOrder));
        };

        UI["SetTint"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(WidgetId, "color", ToCssColor(R, G, B, A));
        };

        UI["SetBackgroundColor"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(WidgetId, "background-color", ToCssColor(R, G, B, A));
        };

        UI["SetTextColor"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(WidgetId, "color", ToCssColor(R, G, B, A));
        };

        UI["SetAlpha"] = [](const FString& WidgetId, float Alpha) -> bool
        {
            return SetStyle(WidgetId, "opacity", std::to_string(std::clamp(Alpha, 0.0f, 1.0f)));
        };

        UI["SetRounding"] = [](const FString& WidgetId, float Rounding) -> bool
        {
            return SetStyle(WidgetId, "border-radius", ToPx(Rounding));
        };

        UI["SetFontScale"] = [](const FString& WidgetId, float FontScale) -> bool
        {
            return SetStyle(WidgetId, "font-size", std::to_string(std::max(FontScale, 0.0f)) + "em");
        };

        UI["SetWidgetTransform"] = [](const FString& WidgetId, float X, float Y, float W, float H) -> bool
        {
            return SetTransformStyles(WidgetId, X, Y, W, H);
        };

        UI["SetWidgetAnchors"] = UnsupportedLegacyOption;
        UI["SetImageOptions"] = UnsupportedLegacyOption;
        UI["SetButtonImages"] = UnsupportedLegacyOption;
        UI["SetProgressImages"] = UnsupportedLegacyOption;
        UI["SetSpriteFrame"] = UnsupportedLegacyOption;
        UI["SetSpriteFrameByMeta"] = UnsupportedLegacyOption;
        UI["AnimateTransform"] = UnsupportedLegacyOption;
        UI["AnimatePatrol"] = UnsupportedLegacyOption;
        UI["AnimatePatrolBySpeed"] = UnsupportedLegacyOption;
        UI["StopAnimation"] = UnsupportedLegacyOption;

        UI["CreatePanel"] = UnsupportedLegacyCreate;
        UI["CreateText"] = UnsupportedLegacyCreate;
        UI["CreateButton"] = UnsupportedLegacyCreate;
        UI["CreateImage"] = UnsupportedLegacyCreate;
        UI["CreateProgressBar"] = UnsupportedLegacyCreate;

        UI["PollActionEvents"] = [](sol::this_state State)
        {
            sol::state_view Lua(State);
            sol::table Events = Lua.create_table();
            if (!GEngine)
            {
                return Events;
            }

            const TArray<FString> PendingEvents = GEngine->PollRmlUIActionEvents();
            int32 Index = 1;
            for (const FString& EventName : PendingEvents)
            {
                Events[Index++] = EventName;
            }
            return Events;
        };

        API["UI"] = UI;
    }
}
