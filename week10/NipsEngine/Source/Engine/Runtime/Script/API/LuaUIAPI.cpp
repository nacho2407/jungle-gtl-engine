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

        UI["SetElementText"] = [](const FString& ElementId, const FString& Text) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementText(ElementId, Text) : false;
        };

        UI["GetElementText"] = [](const FString& ElementId) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementText(ElementId) : "";
        };

        UI["GetElementValue"] = [](const FString& ElementId) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementValue(ElementId) : "";
        };

        UI["SetElementValue"] = [](const FString& ElementId, const FString& Value) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementValue(ElementId, Value) : false;
        };

        UI["HasElement"] = [](const FString& ElementId) -> bool
        {
            return GEngine ? GEngine->HasRmlUIElement(ElementId) : false;
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

        UI["HasElementClass"] = [](const FString& ElementId, const FString& ClassName) -> bool
        {
            return GEngine ? GEngine->HasRmlUIElementClass(ElementId, ClassName) : false;
        };

        UI["GetElementClassNames"] = [](const FString& ElementId) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementClassNames(ElementId) : "";
        };

        UI["SetElementClassNames"] = [](const FString& ElementId, const FString& ClassNames) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementClassNames(ElementId, ClassNames) : false;
        };

        UI["HasElementAttribute"] = [](const FString& ElementId, const FString& Name) -> bool
        {
            return GEngine ? GEngine->HasRmlUIElementAttribute(ElementId, Name) : false;
        };

        UI["GetElementAttribute"] = [](const FString& ElementId, const FString& Name) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementAttribute(ElementId, Name) : "";
        };

        UI["SetElementAttribute"] = [](const FString& ElementId, const FString& Name, const FString& Value) -> bool
        {
            return SetAttribute(ElementId, Name, Value);
        };

        UI["RemoveElementAttribute"] = [](const FString& ElementId, const FString& Name) -> bool
        {
            return GEngine ? GEngine->RemoveRmlUIElementAttribute(ElementId, Name) : false;
        };

        UI["GetElementStyle"] = [](const FString& ElementId, const FString& Name) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementStyle(ElementId, Name) : "";
        };

        UI["SetElementStyle"] = [](const FString& ElementId, const FString& Name, const FString& Value) -> bool
        {
            return SetStyle(ElementId, Name, Value);
        };

        UI["RemoveElementStyle"] = [](const FString& ElementId, const FString& Name) -> bool
        {
            return GEngine ? GEngine->RemoveRmlUIElementStyle(ElementId, Name) : false;
        };

        UI["FocusElement"] = sol::overload(
            [](const FString& ElementId) -> bool
            {
                return GEngine ? GEngine->FocusRmlUIElement(ElementId, false) : false;
            },
            [](const FString& ElementId, bool bFocusVisible) -> bool
            {
                return GEngine ? GEngine->FocusRmlUIElement(ElementId, bFocusVisible) : false;
            });

        UI["IsElementFocused"] = [](const FString& ElementId) -> bool
        {
            return GEngine ? GEngine->IsRmlUIElementFocused(ElementId) : false;
        };

        UI["BlurElement"] = [](const FString& ElementId) -> bool
        {
            return GEngine ? GEngine->BlurRmlUIElement(ElementId) : false;
        };

        UI["ClickElement"] = [](const FString& ElementId) -> bool
        {
            return GEngine ? GEngine->ClickRmlUIElement(ElementId) : false;
        };

        UI["SetText"] = [](const FString& ElementId, const FString& Text) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementText(ElementId, Text) : false;
        };

        UI["GetValue"] = [](const FString& ElementId) -> FString
        {
            return GEngine ? GEngine->GetRmlUIElementValue(ElementId) : "";
        };

        UI["SetValue"] = [](const FString& ElementId, const FString& Value) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementValue(ElementId, Value) : false;
        };

        UI["SetImage"] = [](const FString& ElementId, const FString& ImagePath) -> bool
        {
            return SetAttribute(ElementId, "src", ImagePath);
        };

        UI["SetProgress"] = [](const FString& ElementId, float Value) -> bool
        {
            return SetAttribute(ElementId, "value", std::to_string(Value));
        };

        UI["SetVisible"] = [](const FString& ElementId, bool bVisible) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementVisible(ElementId, bVisible) : false;
        };

        UI["SetEnabled"] = [](const FString& ElementId, bool bEnabled) -> bool
        {
            return GEngine ? GEngine->SetRmlUIElementEnabled(ElementId, bEnabled) : false;
        };

        UI["SetActionEvent"] = [](const FString& ElementId, const FString& EventName) -> bool
        {
            return SetAttribute(ElementId, "data-action", EventName);
        };

        UI["RemoveElement"] = [](const FString& ElementId) -> bool
        {
            bool bResult = GEngine ? GEngine->SetRmlUIElementVisible(ElementId, false) : false;
            bResult = SetAttribute(ElementId, "disabled", "true") || bResult;
            return bResult;
        };

        UI["SetZOrder"] = [](const FString& ElementId, int32 ZOrder) -> bool
        {
            return SetStyle(ElementId, "z-index", std::to_string(ZOrder));
        };

        UI["SetTint"] = [](const FString& ElementId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(ElementId, "color", ToCssColor(R, G, B, A));
        };

        UI["SetBackgroundColor"] = [](const FString& ElementId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(ElementId, "background-color", ToCssColor(R, G, B, A));
        };

        UI["SetTextColor"] = [](const FString& ElementId, float R, float G, float B, float A) -> bool
        {
            return SetStyle(ElementId, "color", ToCssColor(R, G, B, A));
        };

        UI["SetAlpha"] = [](const FString& ElementId, float Alpha) -> bool
        {
            return SetStyle(ElementId, "opacity", std::to_string(std::clamp(Alpha, 0.0f, 1.0f)));
        };

        UI["SetRounding"] = [](const FString& ElementId, float Rounding) -> bool
        {
            return SetStyle(ElementId, "border-radius", ToPx(Rounding));
        };

        UI["SetFontScale"] = [](const FString& ElementId, float FontScale) -> bool
        {
            return SetStyle(ElementId, "font-size", std::to_string(std::max(FontScale, 0.0f)) + "em");
        };

        UI["SetElementTransform"] = [](const FString& ElementId, float X, float Y, float W, float H) -> bool
        {
            return SetTransformStyles(ElementId, X, Y, W, H);
        };

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
