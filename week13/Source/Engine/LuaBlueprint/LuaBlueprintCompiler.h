#pragma once

#include "Core/Types/CoreTypes.h"
#include "LuaBlueprint/LuaBlueprintTypes.h"

class ULuaBlueprintAsset;
struct FLuaBlueprintNode;
struct FLuaBlueprintPin;

struct FLuaBlueprintCompileResult
{
	bool bSuccess = false;
	FString GeneratedLuaSource;
	TArray<FLuaBlueprintDiagnostic> Diagnostics;
};

class FLuaBlueprintCompiler
{
public:
	static FLuaBlueprintCompileResult Compile(const ULuaBlueprintAsset& Asset);
};
