#include "Core/ImportedMaterialPolicy.h"

#include "Core/Paths.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace
{
	FString SanitizeAssetToken(FString Token)
	{
		for (char& Ch : Token)
		{
			const bool bAlphaNum =
				(Ch >= '0' && Ch <= '9') ||
				(Ch >= 'A' && Ch <= 'Z') ||
				(Ch >= 'a' && Ch <= 'z');

			if (!bAlphaNum && Ch != '_' && Ch != '-')
			{
				Ch = '_';
			}
		}

		return Token.empty() ? FString("Material") : Token;
	}

	FString TrimAscii(FString Value)
	{
		const size_t Start = Value.find_first_not_of(" \t\r\n");
		if (Start == FString::npos)
		{
			return {};
		}

		const size_t End = Value.find_last_not_of(" \t\r\n");
		return Value.substr(Start, End - Start + 1);
	}
}

FString FImportedMaterialPolicy::MakeImportedMaterialAssetName(const FString& SourceMtlPath, int32 MaterialIndex)
{
	const std::filesystem::path SourcePath(FPaths::ToWide(FPaths::Normalize(SourceMtlPath)));
	const FString SourceStem = SanitizeAssetToken(FPaths::ToUtf8(SourcePath.stem().wstring()));
	return SourceStem + "_Mat_" + std::to_string(MaterialIndex);
}

FString FImportedMaterialPolicy::MakeMaterialSlotAliasKey(const FString& SourcePath, const FString& SlotName)
{
	return FPaths::Normalize(SourcePath) + "::" + SlotName;
}

FString FImportedMaterialPolicy::ResolveObjMaterialLibraryPath(const FString& ObjPath)
{
	std::filesystem::path AbsoluteObjPath(FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(ObjPath))));
	std::ifstream ObjFile(AbsoluteObjPath);
	if (!ObjFile.is_open())
	{
		return {};
	}

	FString Line;
	while (std::getline(ObjFile, Line))
	{
		Line = TrimAscii(Line);
		if (Line.rfind("mtllib ", 0) != 0)
		{
			continue;
		}

		FString MtlReference = TrimAscii(Line.substr(7));
		if (MtlReference.empty())
		{
			return {};
		}

		std::filesystem::path MtlPath(FPaths::ToWide(MtlReference));
		if (MtlPath.is_relative())
		{
			MtlPath = AbsoluteObjPath.parent_path() / MtlPath;
		}

		std::error_code Ec;
		MtlPath = MtlPath.lexically_normal();
		if (!std::filesystem::exists(MtlPath, Ec) || Ec)
		{
			return {};
		}

		std::filesystem::path RelativePath = std::filesystem::relative(MtlPath, std::filesystem::path(FPaths::RootDir()), Ec);
		if (Ec)
		{
			return FPaths::ToUtf8(MtlPath.generic_wstring());
		}

		return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
	}

	return {};
}

TArray<FString> FImportedMaterialPolicy::CollectObjMaterialSlotNames(const FString& ObjPath)
{
	TArray<FString> SlotNames;
	std::unordered_set<FString> SeenSlotNames;

	std::filesystem::path AbsoluteObjPath(FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(ObjPath))));
	std::ifstream ObjFile(AbsoluteObjPath);
	if (!ObjFile.is_open())
	{
		return SlotNames;
	}

	FString Line;
	while (std::getline(ObjFile, Line))
	{
		Line = TrimAscii(Line);
		if (Line.rfind("usemtl ", 0) != 0)
		{
			continue;
		}

		FString SlotName = TrimAscii(Line.substr(7));
		if (!SlotName.empty() && SeenSlotNames.insert(SlotName).second)
		{
			SlotNames.push_back(SlotName);
		}
	}

	return SlotNames;
}
