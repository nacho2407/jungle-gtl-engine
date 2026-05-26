#include "Asset/CurveAssetLoader.h"

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
	FString NormalizeCurvePath(const FString& Path)
	{
		return FPaths::Normalize(Path);
	}

	bool IsCurveAssetPath(const FString& Path)
	{
		FString LowerPath = FPaths::Normalize(Path);
		std::transform(
			LowerPath.begin(),
			LowerPath.end(),
			LowerPath.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});

		return std::filesystem::path(FPaths::ToWide(LowerPath)).extension() == L".curve";
	}

	bool LoadCurveJson(const FString& Path, json::JSON& OutRoot)
	{
		const FString NormalizedPath = NormalizeCurvePath(Path);
		if (NormalizedPath.empty() || !IsCurveAssetPath(NormalizedPath))
		{
			return false;
		}

		std::ifstream CurveFile(FPaths::ToWide(NormalizedPath));
		if (!CurveFile.is_open())
		{
			UE_LOG_ERROR("[CurveAssetLoader] Failed to open curve asset: %s", NormalizedPath.c_str());
			return false;
		}

		FString FileContent((std::istreambuf_iterator<char>(CurveFile)), std::istreambuf_iterator<char>());
		OutRoot = json::JSON::Load(FileContent);
		if (OutRoot.JSONType() != json::JSON::Class::Object)
		{
			UE_LOG_ERROR("[CurveAssetLoader] Invalid curve asset json: %s", NormalizedPath.c_str());
			return false;
		}

		return true;
	}

	template <typename TCurveAsset>
	TCurveAsset* LoadCurveAsset(const FString& Path)
	{
		const FString NormalizedPath = NormalizeCurvePath(Path);

		json::JSON Root;
		if (!LoadCurveJson(NormalizedPath, Root))
		{
			return nullptr;
		}

		TCurveAsset* Curve = UObjectManager::Get().CreateObject<TCurveAsset>();
		FJsonReader Reader(Root);
		Curve->Serialize(Reader);
		Curve->SetAssetPath(NormalizedPath);
		Curve->GetMutableCurve().SortKeys();
		return Curve;
	}

	template <typename TCurveAsset>
	bool SaveCurveAsset(const FString& Path, const TCurveAsset* Curve)
	{
		if (!Curve)
		{
			return false;
		}

		const FString NormalizedPath = NormalizeCurvePath(Path);
		if (NormalizedPath.empty() || !IsCurveAssetPath(NormalizedPath))
		{
			return false;
		}

		json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
		FJsonWriter Writer(Root);
		const_cast<TCurveAsset*>(Curve)->Serialize(Writer);

		std::error_code ErrorCode;
		std::filesystem::path FilePath(FPaths::ToWide(NormalizedPath));
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

		std::ofstream OutFile(FilePath);
		if (!OutFile.is_open())
		{
			UE_LOG_ERROR("[CurveAssetLoader] Failed to open curve asset for writing: %s", NormalizedPath.c_str());
			return false;
		}

		OutFile << Root.dump(4);
		return true;
	}
}

UCurveFloatAsset* FCurveAssetLoader::Load(const FString& Path) const
{
	return LoadFloat(Path);
}

UCurveFloatAsset* FCurveAssetLoader::LoadFloat(const FString& Path) const
{
	return LoadCurveAsset<UCurveFloatAsset>(Path);
}

UCurveVectorAsset* FCurveAssetLoader::LoadVector(const FString& Path) const
{
	return LoadCurveAsset<UCurveVectorAsset>(Path);
}

UCurveColorAsset* FCurveAssetLoader::LoadColor(const FString& Path) const
{
	return LoadCurveAsset<UCurveColorAsset>(Path);
}

bool FCurveAssetLoader::Save(const FString& Path, const UCurveFloatAsset* Curve) const
{
	return SaveCurveAsset(Path, Curve);
}

bool FCurveAssetLoader::Save(const FString& Path, const UCurveVectorAsset* Curve) const
{
	return SaveCurveAsset(Path, Curve);
}

bool FCurveAssetLoader::Save(const FString& Path, const UCurveColorAsset* Curve) const
{
	return SaveCurveAsset(Path, Curve);
}

bool FCurveAssetLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == ".curve" || Extension == "curve";
}
