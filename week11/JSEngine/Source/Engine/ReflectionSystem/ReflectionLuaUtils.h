#pragma once

#include "Core/Containers/String.h"
#include "Object/FName.h"
#include "ReflectionSystem/ReflectionTypeInfo.h"
#include "ReflectionSystem/ReflectedProperty.h"
#include "ThirdParty/sol/sol.hpp"

class UObject;

namespace ReflectionLuaUtils
{
struct FResolvedProperty
{
    FProperty* Property = nullptr;
    void* ValuePtr = nullptr;
    FClassInfo* OwnerClass = nullptr;

    bool IsValid() const
    {
        return Property != nullptr && ValuePtr != nullptr;
    }
};

FResolvedProperty FindProperty(UObject* Object, const FString& PropertyName);

sol::object GetProperty(sol::this_state State, UObject* Object, const FString& PropertyName);

bool SetProperty(UObject* Object, const FString& PropertyName, const sol::object& Value);

sol::object PropertyValueToLua(sol::this_state State, FProperty* Property, void* ValuePtr);

bool LuaToPropertyValue(FProperty* Property, void* ValuePtr, const sol::object& Value);
} // namespace ReflectionLuaUtils