#pragma once

#include "Core/Types/CoreTypes.h"

#include <sol/sol.hpp>
#include <utility>

struct FLuaDocMember
{
	FString Name;
	FString Signature;
	bool bProperty = false;
};

struct FLuaDocType
{
	FString Name;
	FString BaseName;
	TArray<FLuaDocMember> Members;

	FLuaDocType& Property(const FString& InName, const FString& InType);
	FLuaDocType& Method(const FString& InSignature);
};

template<typename T>
class TLuaTypeBinding
{
public:
	TLuaTypeBinding(sol::usertype<T> InUserType, FLuaDocType& InDoc)
		: UserType(InUserType)
		, Doc(InDoc)
	{
	}

	template<typename Func>
	TLuaTypeBinding& Method(const char* Name, const char* Signature, Func&& Fn)
	{
		UserType.set_function(Name, std::forward<Func>(Fn));
		Doc.Method(Signature);
		return *this;
	}

	template<typename Getter, typename Setter>
	TLuaTypeBinding& Property(const char* Name, const char* LuaType, Getter&& Get, Setter&& Set)
	{
		UserType[Name] = sol::property(std::forward<Getter>(Get), std::forward<Setter>(Set));
		Doc.Property(Name, LuaType);
		return *this;
	}

	template<typename Getter>
	TLuaTypeBinding& ReadonlyProperty(const char* Name, const char* LuaType, Getter&& Get)
	{
		UserType[Name] = sol::property(std::forward<Getter>(Get));
		Doc.Property(Name, LuaType);
		return *this;
	}

private:
	sol::usertype<T> UserType;
	FLuaDocType& Doc;
};

class FLuaDocRegistry
{
public:
	static FLuaDocRegistry& Get();

	FLuaDocType& Type(const FString& Name, const FString& BaseName = "");
	void Global(const FString& Name, const FString& Type);
	void Function(const FString& Name, const FString& Signature);
	void GenerateStubs() const;
	void Reset();

	template<typename T, typename... Args>
	TLuaTypeBinding<T> BindType(sol::state& Lua, const char* Name, Args&&... ExtraArgs)
	{
		FLuaDocType& Doc = Type(Name);
		return TLuaTypeBinding<T>(
			Lua.new_usertype<T>(Name, std::forward<Args>(ExtraArgs)...),
			Doc);
	}

	template<typename T, typename... Args>
	TLuaTypeBinding<T> BindDerivedType(sol::state& Lua, const char* Name, const char* BaseName, Args&&... ExtraArgs)
	{
		FLuaDocType& Doc = Type(Name, BaseName);
		return TLuaTypeBinding<T>(
			Lua.new_usertype<T>(Name, std::forward<Args>(ExtraArgs)...),
			Doc);
	}

	template<typename T>
	TLuaTypeBinding<T> ExtendType(sol::state& Lua, const char* Name)
	{
		FLuaDocType& Doc = Type(Name);
		return TLuaTypeBinding<T>(Lua[Name].get<sol::usertype<T>>(), Doc);
	}

private:
	TArray<FLuaDocType> Types;
	TArray<FString> Globals;
	TArray<FString> Functions;
};
