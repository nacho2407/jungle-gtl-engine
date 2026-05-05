#include "Editor/Subsystem/AssetFactory.h"

#include "FloatCurve/FloatCurveAsset.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"

#include <filesystem>

namespace
{
	FString SanitizeAssetStem(const FString& AssetName)
	{
		return AssetName.empty() ? FString("NewFloatCurve") : AssetName;
	}

	std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path& Directory, const FString& AssetName, const wchar_t* Extension)
	{
		const FString BaseStem = SanitizeAssetStem(AssetName);

		int32 Suffix = 0;
		for (;;)
		{
			FString CandidateStem = BaseStem;
			if (Suffix > 0)
			{
				CandidateStem += "_";
				CandidateStem += std::to_string(Suffix);
			}

			std::filesystem::path CandidatePath = Directory / (FPaths::ToWide(CandidateStem) + Extension);
			if (!std::filesystem::exists(CandidatePath))
			{
				return CandidatePath;
			}

			++Suffix;
		}
	}
}

bool FAssetFactory::CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".curve");

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FFloatCurve& Curve = NewAsset->GetCurve();
	Curve.Reset();
	Curve.AddKey(0.0f, 0.0f);
	Curve.AddKey(1.0f, 1.0f);
	Curve.SortKeys();

	const bool bSaved = NewAsset->SaveToFile(NewAsset->GetSourcePath());
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}
