#include "Core/MaterialLoadService.h"

#include "Core/FbxMaterialLoadService.h"
#include "Core/ImportedMaterialPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Resource/ObjMtlLoader.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace fs = std::filesystem;

FMaterialLoadService::FMaterialLoadService(FResourceManager& InResourceManager)
	: ResourceManager(InResourceManager)
{
}

bool FMaterialLoadService::Load(const FString& MtlFilePath, EMaterialShaderType ShaderType, ID3D11Device* Device)
{
	const FString NormalizedMtlFilePath = FPaths::Normalize(MtlFilePath);
	if (NormalizedMtlFilePath.empty())
	{
		return false;
	}

	std::filesystem::path RequestedPath(FPaths::ToWide(NormalizedMtlFilePath));
	std::wstring RequestedExtension = RequestedPath.extension().wstring();
	std::transform(RequestedExtension.begin(), RequestedExtension.end(), RequestedExtension.begin(), ::towlower);
	if (RequestedExtension == L".obj")
	{
		const FString ResolvedMtlPath = FImportedMaterialPolicy::ResolveObjMaterialLibraryPath(NormalizedMtlFilePath);
		if (ResolvedMtlPath.empty())
		{
			return false;
		}

		const bool bLoadedMaterial = Load(ResolvedMtlPath, ShaderType, Device);
		if (bLoadedMaterial)
		{
			ResourceManager.RegisterObjMaterialSlotAliases(NormalizedMtlFilePath, ResolvedMtlPath);
		}

		return bLoadedMaterial;
	}

	if (RequestedExtension == L".fbx")
	{
		return FFbxMaterialLoadService(ResourceManager).Load(NormalizedMtlFilePath, ShaderType, Device);
	}

	TMap<FString, UMaterial*> Parsed;
	TArray<FString> MaterialOrder;
	if (!FObjMtlLoader::Load(NormalizedMtlFilePath, Parsed, ResourceManager.CachedDevice.Get(), &MaterialOrder))
	{
		UE_LOG_WARNING("Failed to load MTL: %s", NormalizedMtlFilePath.c_str());
		return false;
	}

	if (Parsed.empty())
	{
		UMaterial* DefaultMat = ResourceManager.GetMaterial("DefaultWhite");
		if (!DefaultMat)
		{
			UE_LOG_WARNING("DefaultWhite material not found.");
			return false;
		}

		Parsed["DefaultWhite"] = DefaultMat;
		MaterialOrder.push_back("DefaultWhite");
	}

	const fs::path SourceMtlPath(FPaths::ToWide(NormalizedMtlFilePath));
	const bool bCanPromoteMtlToMaterialAssets = SourceMtlPath.extension() == L".mtl";
	const fs::path AutoMaterialDir = fs::path(L"Asset") / L"Material" / L"Auto";

	for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialOrder.size()); ++MaterialIndex)
	{
		const FString& Name = MaterialOrder[MaterialIndex];
		auto ParsedIt = Parsed.find(Name);
		if (ParsedIt == Parsed.end())
		{
			continue;
		}

		UMaterial* Mat = ParsedIt->second;
		if (!Mat)
		{
			continue;
		}

		FString MaterialAssetPath = NormalizedMtlFilePath;
		FString MaterialKey = Name;

		if (bCanPromoteMtlToMaterialAssets)
		{
			const FString MaterialName = FImportedMaterialPolicy::MakeImportedMaterialAssetName(NormalizedMtlFilePath, MaterialIndex);
			const fs::path RelativeMatPath =
				AutoMaterialDir / FPaths::ToWide(MaterialName + ".mat");

			MaterialAssetPath = FPaths::Normalize(FPaths::ToUtf8(RelativeMatPath.generic_wstring()));
			Mat->Name = MaterialName;
			MaterialKey = MaterialAssetPath;
		}

		if (Mat->ImportedName.empty())
		{
			Mat->ImportedName = Name;
		}
		Mat->FilePath = MaterialAssetPath;
		Mat->SetShaderType(ShaderType);

		UMaterial* ExistingMaterial = ResourceManager.MaterialCache.FindMaterialByKey(MaterialKey);
		if (ExistingMaterial)
		{
			if (ExistingMaterial != Mat)
			{
				UObjectManager::Get().DestroyObject(Mat);
				Mat = ExistingMaterial;
			}
		}
		else
		{
			ResourceManager.MaterialCache.RegisterMaterial(MaterialKey, Mat);
		}

		if (!ResourceManager.MaterialCache.ContainsMaterialKey(Mat->Name))
		{
			ResourceManager.MaterialCache.RegisterMaterial(Mat->Name, Mat);
		}

		if (!ResourceManager.MaterialCache.ContainsMaterialKey(Name))
		{
			ResourceManager.MaterialCache.RegisterMaterial(Name, Mat);
		}

		ResourceManager.MaterialCache.SetMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedMtlFilePath, Name), MaterialKey);

		FMaterial& MaterialData = Mat->MaterialData;

		if (MaterialData.bHasDiffuseTexture && !MaterialData.DiffuseTexPath.empty())
		{
			ResourceManager.LoadTexture(MaterialData.DiffuseTexPath, ResourceManager.CachedDevice.Get());
		}

		if (MaterialData.bHasAmbientTexture && !MaterialData.AmbientTexPath.empty())
		{
			ResourceManager.LoadTexture(MaterialData.AmbientTexPath, ResourceManager.CachedDevice.Get());
		}

		if (MaterialData.bHasSpecularTexture && !MaterialData.SpecularTexPath.empty())
		{
			ResourceManager.LoadTexture(MaterialData.SpecularTexPath, ResourceManager.CachedDevice.Get());
		}

		if (MaterialData.bHasBumpTexture && !MaterialData.BumpTexPath.empty())
		{
			ResourceManager.LoadTexture(MaterialData.BumpTexPath, ResourceManager.CachedDevice.Get());
		}

		if (bCanPromoteMtlToMaterialAssets && !MaterialAssetPath.empty())
		{
			const fs::path AbsoluteMatPath =
				fs::path(FPaths::RootDir()) / FPaths::ToWide(MaterialAssetPath);

			std::error_code Ec;
			fs::create_directories(AbsoluteMatPath.parent_path(), Ec);

			ResourceManager.SerializeMaterial(MaterialAssetPath, Mat);
		}
	}

	UE_LOG("Loaded MTL: %s", NormalizedMtlFilePath.c_str());
	return true;
}
