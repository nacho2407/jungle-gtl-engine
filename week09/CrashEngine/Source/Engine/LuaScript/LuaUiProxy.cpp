#include "LuaScript/LuaUiProxy.h"

#include "Core/Logging/LogMacros.h"
#include "Runtime/Engine.h"
#include "UI/RmlUiManager.h"

namespace
{
FRmlUiManager* GetRmlUiManager()
{
    return GEngine ? GEngine->GetRmlUiManager() : nullptr;
}

FLuaUiDocumentProxy LoadDocument(const FString& Path, const FString& Name)
{
    FRmlUiManager* Manager = GetRmlUiManager();
    if (!Manager)
    {
        UE_LOG(Lua, Warning, "UI.Load failed. RmlUi manager is not available.");
        return FLuaUiDocumentProxy();
    }

    const FString DocumentName = Name.empty() ? Path : Name;
    if (!Manager->LoadDocument(DocumentName, Path))
    {
        UE_LOG(Lua, Warning, "UI.Load failed. name=%s path=%s", DocumentName.c_str(), Path.c_str());
        return FLuaUiDocumentProxy();
    }

    return FLuaUiDocumentProxy(DocumentName);
}
} // namespace

FLuaUiDocumentProxy::FLuaUiDocumentProxy(FString InName)
    : Name(std::move(InName))
{
}

FRmlUiManager* FLuaUiDocumentProxy::GetManager() const
{
    return GetRmlUiManager();
}

bool FLuaUiDocumentProxy::IsValid() const
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->IsDocumentLoaded(Name);
}

bool FLuaUiDocumentProxy::Show()
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->ShowDocument(Name, true);
}

bool FLuaUiDocumentProxy::Hide()
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->ShowDocument(Name, false);
}

bool FLuaUiDocumentProxy::Close()
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->CloseDocument(Name);
}

bool FLuaUiDocumentProxy::SetPosition(float X, float Y)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetDocumentPosition(Name, X, Y);
}

bool FLuaUiDocumentProxy::SetSize(float Width, float Height)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetDocumentSize(Name, Width, Height);
}

bool FLuaUiDocumentProxy::SetScale(float Scale)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetDocumentScale(Name, Scale);
}

bool FLuaUiDocumentProxy::SetZOrder(int32 ZOrder)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetDocumentZOrder(Name, ZOrder);
}

bool FLuaUiDocumentProxy::SetLayout(float X, float Y, float Width, float Height, float Scale, int32 ZOrder)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetDocumentLayout(Name, X, Y, Width, Height, Scale, ZOrder);
}

bool FLuaUiDocumentProxy::SetText(const FString& ElementId, const FString& Text)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetElementText(Name, ElementId, Text);
}

bool FLuaUiDocumentProxy::SetClass(const FString& ElementId, const FString& ClassName, bool bEnabled)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetElementClass(Name, ElementId, ClassName, bEnabled);
}

bool FLuaUiDocumentProxy::SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetElementProperty(Name, ElementId, PropertyName, Value);
}

bool FLuaUiDocumentProxy::SetTexture(const FString& ElementId, const FString& TexturePath)
{
    FRmlUiManager* Manager = GetManager();
    return Manager && Manager->SetElementTexture(Name, ElementId, TexturePath);
}

void LuaUiProxy::Bind(sol::state& Lua)
{
    Lua.new_usertype<FLuaUiDocumentProxy>("UIDocument", sol::no_constructor,
        "Name", sol::property(&FLuaUiDocumentProxy::GetName),
        "IsValid", &FLuaUiDocumentProxy::IsValid,
        "Show", &FLuaUiDocumentProxy::Show,
        "Hide", &FLuaUiDocumentProxy::Hide,
        "Close", &FLuaUiDocumentProxy::Close,
        "SetPosition", &FLuaUiDocumentProxy::SetPosition,
        "SetSize", &FLuaUiDocumentProxy::SetSize,
        "SetScale", &FLuaUiDocumentProxy::SetScale,
        "SetZOrder", &FLuaUiDocumentProxy::SetZOrder,
        "SetLayout", &FLuaUiDocumentProxy::SetLayout,
        "SetText", &FLuaUiDocumentProxy::SetText,
        "SetClass", &FLuaUiDocumentProxy::SetClass,
        "SetProperty", &FLuaUiDocumentProxy::SetProperty,
        "SetTexture", &FLuaUiDocumentProxy::SetTexture
    );

    sol::table Ui = Lua.create_table();
    Ui.set_function("Load", sol::overload(
        [](const FString& Path) {
            return LoadDocument(Path, Path);
        },
        [](const FString& Path, const FString& Name) {
            return LoadDocument(Path, Name);
        }
    ));
    Ui.set_function("Get", [](const FString& Name) {
        return FLuaUiDocumentProxy(Name);
    });
    Ui.set_function("Close", [](const FString& Name) {
        FRmlUiManager* Manager = GetRmlUiManager();
        return Manager && Manager->CloseDocument(Name);
    });
    Ui.set_function("Show", [](const FString& Name) {
        FRmlUiManager* Manager = GetRmlUiManager();
        return Manager && Manager->ShowDocument(Name, true);
    });
    Ui.set_function("Hide", [](const FString& Name) {
        FRmlUiManager* Manager = GetRmlUiManager();
        return Manager && Manager->ShowDocument(Name, false);
    });

    Lua["UI"] = Ui;
}
