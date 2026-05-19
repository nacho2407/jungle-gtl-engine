#pragma once

#include "Core/Containers/String.h"
#include "Core/Logging/Log.h"
#include "Object/Property.h"
#include "ThirdParty/sol/sol.hpp"

class UObject;
class UFunction;

class FLuaPropertyValueCodec
{
public:
	static bool WriteLuaValueToProperty(sol::object Value, const FProperty& Property, void* ValuePtr);
	static sol::object ReadPropertyToLuaValue(sol::this_state State, const FProperty& Property, const void* ValuePtr);
};

class FLuaReflectionFunctionBridge
{
public:
	static sol::object CallByName(sol::this_state State, UObject* Object, const FString& FunctionName, sol::variadic_args Args);
	static sol::object CallFunction(sol::this_state State, UObject* Object, const UFunction* Function, sol::variadic_args Args);
};

class FLuaBindingRegistry
{
public:
	template <typename T>
	static bool RegisterClassProperty(
		sol::state& Lua,
		const char* LuaTypeName,
		const char* PropertyName,
		const FProperty* Property,
		bool bWritable)
	{
		if (!LuaTypeName || !PropertyName || !Property)
		{
			return false;
		}
		if (!HasPropertyFlag(Property->Flags, EPropertyFlags::LuaReadOnly)
			&& !HasPropertyFlag(Property->Flags, EPropertyFlags::LuaReadWrite))
		{
			return false;
		}
		bWritable = bWritable && HasPropertyFlag(Property->Flags, EPropertyFlags::LuaReadWrite);

		sol::object TypeObj = Lua[LuaTypeName];
		if (!TypeObj.valid() || TypeObj == sol::nil)
		{
			UE_LOG_WARNING("[LuaBinding] Lua type not found: %s", LuaTypeName);
			return false;
		}

		sol::table TypeTable = TypeObj.as<sol::table>();
		sol::object Existing = TypeTable[PropertyName];
		if (Existing.valid() && Existing != sol::nil)
		{
			UE_LOG_WARNING("[LuaBinding] Overwriting Lua property %s.%s", LuaTypeName, PropertyName);
		}

		lua_State* State = Lua.lua_state();
		sol::usertype<T> UserType = TypeObj.as<sol::usertype<T>>();
		if (bWritable)
		{
			UserType.set(PropertyName, sol::property(
				[Property, State](T& Self) -> sol::object
				{
					return FLuaPropertyValueCodec::ReadPropertyToLuaValue(State, *Property, Property->GetValuePtr(&Self));
				},
				[Property](T& Self, sol::object Value)
				{
					if (FLuaPropertyValueCodec::WriteLuaValueToProperty(Value, *Property, Property->GetValuePtr(&Self)))
					{
						Self.PostEditProperty(Property->Name);
					}
				}));
		}
		else
		{
			UserType.set(PropertyName, sol::property(
				[Property, State](T& Self) -> sol::object
				{
					return FLuaPropertyValueCodec::ReadPropertyToLuaValue(State, *Property, Property->GetValuePtr(&Self));
				}));
		}

		return true;
	}

	template <typename T>
	static bool RegisterClassFunction(
		sol::state& Lua,
		const char* LuaTypeName,
		const char* FunctionName,
		sol::object(*Wrapper)(sol::this_state, T*, sol::variadic_args))
	{
		if (!LuaTypeName || !FunctionName || !Wrapper)
		{
			return false;
		}

		sol::object TypeObj = Lua[LuaTypeName];
		if (!TypeObj.valid() || TypeObj == sol::nil)
		{
			UE_LOG_WARNING("[LuaBinding] Lua type not found: %s", LuaTypeName);
			return false;
		}

		sol::table TypeTable = TypeObj.as<sol::table>();
		sol::object Existing = TypeTable[FunctionName];
		if (Existing.valid() && Existing != sol::nil)
		{
			UE_LOG_ERROR("[LuaBinding] Refusing to overwrite Lua function %s.%s", LuaTypeName, FunctionName);
			return false;
		}

		sol::usertype<T> UserType = TypeObj.as<sol::usertype<T>>();
		UserType.set(FunctionName, Wrapper);
		return true;
	}
};

void RegisterAllGeneratedLuaBindings(sol::state& Lua);
