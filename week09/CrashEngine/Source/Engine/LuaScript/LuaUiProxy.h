#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaIncludes.h"

class FRmlUiManager;

class FLuaUiDocumentProxy
{
public:
    FLuaUiDocumentProxy() = default;
    explicit FLuaUiDocumentProxy(FString InName);

    const FString& GetName() const { return Name; }
    bool IsValid() const;

    bool Show();
    bool Hide();
    bool Close();

    bool SetPosition(float X, float Y);
    bool SetSize(float Width, float Height);
    bool SetScale(float Scale);
    bool SetZOrder(int32 ZOrder);
    bool SetLayout(float X, float Y, float Width, float Height, float Scale, int32 ZOrder);

    bool SetText(const FString& ElementId, const FString& Text);
    bool SetTextColor(const FString& ElementId, const FString& Text, int32 R, int32 G, int32 B);
    bool SetClass(const FString& ElementId, const FString& ClassName, bool bEnabled);
    bool SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value);
    bool SetTexture(const FString& ElementId, const FString& TexturePath);

private:
    FRmlUiManager* GetManager() const;

private:
    FString Name;
};

namespace LuaUiProxy
{
void Bind(sol::state& Lua);
}
