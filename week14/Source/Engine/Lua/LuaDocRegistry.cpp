#include "LuaDocRegistry.h"

#include "Core/Logging/Log.h"
#include "Platform/Paths.h"

#include <fstream>

FLuaDocType& FLuaDocType::Property(const FString& InName, const FString& InType)
{
	Members.push_back({ InName, InType, true });
	return *this;
}

FLuaDocType& FLuaDocType::Method(const FString& InSignature)
{
	Members.push_back({ "", InSignature, false });
	return *this;
}

FLuaDocRegistry& FLuaDocRegistry::Get()
{
	static FLuaDocRegistry Instance;
	return Instance;
}

FLuaDocType& FLuaDocRegistry::Type(const FString& Name, const FString& BaseName)
{
	for (FLuaDocType& Existing : Types)
	{
		if (Existing.Name == Name)
		{
			if (Existing.BaseName.empty() && !BaseName.empty())
			{
				Existing.BaseName = BaseName;
			}
			return Existing;
		}
	}

	FLuaDocType& NewType = Types.emplace_back();
	NewType.Name = Name;
	NewType.BaseName = BaseName;
	return NewType;
}

void FLuaDocRegistry::Global(const FString& Name, const FString& Type)
{
	Globals.push_back("---@type " + Type + "\n" + Name + " = " + Name + "\n");
}

void FLuaDocRegistry::Function(const FString& Name, const FString& Signature)
{
	Functions.push_back("---@type " + Signature + "\n" + Name + " = " + Name + "\n");
}

void FLuaDocRegistry::GenerateStubs() const
{
	const std::wstring DefDir = FPaths::Combine(FPaths::ScriptDir(), L".lua_defs");
	FPaths::CreateDir(DefDir);

	const std::wstring StubPath = FPaths::Combine(DefDir, L"generated.lua");
	std::ofstream Out(StubPath, std::ios::binary | std::ios::trunc);
	if (!Out)
	{
		UE_LOG("[LuaDoc] Failed to write generated.lua");
		return;
	}

	Out << "---@meta\n\n";
	Out << "-- Auto-generated from C++ Lua binding metadata.\n\n";

	for (const FLuaDocType& Type : Types)
	{
		Out << "---@class " << Type.Name;
		if (!Type.BaseName.empty())
		{
			Out << ": " << Type.BaseName;
		}
		Out << "\n";

		for (const FLuaDocMember& Member : Type.Members)
		{
			if (Member.bProperty)
			{
				Out << "---@field " << Member.Name << " " << Member.Signature << "\n";
			}
		}

		Out << Type.Name << " = {}\n\n";

		for (const FLuaDocMember& Member : Type.Members)
		{
			if (!Member.bProperty)
			{
				Out << Member.Signature << "\n\n";
			}
		}
	}

	for (const FString& Global : Globals)
	{
		Out << Global << "\n";
	}

	for (const FString& Fn : Functions)
	{
		Out << Fn << "\n";
	}

	UE_LOG("[LuaDoc] Generated Content/Script/.lua_defs/generated.lua");
}

void FLuaDocRegistry::Reset()
{
	Types.clear();
	Globals.clear();
	Functions.clear();
}
