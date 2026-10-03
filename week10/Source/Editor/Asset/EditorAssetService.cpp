#include "Editor/Asset/EditorAssetService.h"

#include "Asset/AssetQueryService.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <utility>

namespace
{
	void AddUniquePath(TArray<FString>& Paths, const FString& Path)
	{
		const FString NormalizedPath = FPaths::Normalize(Path);
		if (!NormalizedPath.empty() && std::find(Paths.begin(), Paths.end(), NormalizedPath) == Paths.end())
		{
			Paths.push_back(NormalizedPath);
		}
	}

	FString ToDisplayName(const FString& Path)
	{
		const std::filesystem::path FsPath(FPaths::ToWide(Path));
		return FPaths::ToUtf8(FsPath.filename().wstring());
	}

	FString ToProjectRelativePath(const std::filesystem::path& AbsolutePath)
	{
		std::error_code Ec;
		std::filesystem::path Relative = std::filesystem::relative(AbsolutePath, std::filesystem::path(FPaths::RootDir()), Ec);
		if (Ec)
		{
			Relative = AbsolutePath.lexically_normal();
		}
		return FPaths::Normalize(FPaths::ToUtf8(Relative.generic_wstring()));
	}

	FString LowerExtension(const std::filesystem::path& Path)
	{
		FString Extension = FPaths::ToUtf8(Path.extension().wstring());
		std::transform(
			Extension.begin(),
			Extension.end(),
			Extension.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});
		return Extension;
	}

	bool ExtensionMatches(const FString& Extension, std::initializer_list<const char*> Candidates)
	{
		for (const char* Candidate : Candidates)
		{
			if (Extension == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	void ListAssetFiles(const std::filesystem::path& SubDirectory, std::initializer_list<const char*> Extensions, TArray<FString>& OutPaths)
	{
		const std::filesystem::path Root = (std::filesystem::path(FPaths::RootDir()) / L"Asset" / SubDirectory).lexically_normal();
		if (!std::filesystem::exists(Root))
		{
			return;
		}

		std::error_code Ec;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Ec))
		{
			if (Ec)
			{
				break;
			}
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const FString Extension = LowerExtension(Entry.path());
			if (ExtensionMatches(Extension, Extensions))
			{
				AddUniquePath(OutPaths, ToProjectRelativePath(Entry.path()));
			}
		}
	}
}

void FEditorAssetService::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
	RefreshAssetDatabase();
}

void FEditorAssetService::RefreshAssetDatabase()
{
	StaticMeshPaths.clear();
	SkeletalMeshPaths.clear();
	TexturePaths.clear();
	MaterialInterfaceNames.clear();
	FontNames.clear();
	ParticleNames.clear();
	CachedMaterialInterfaces.clear();
	CachedMaterialInterfaceResolved.clear();

	for (const FString& Path : FAssetQueryService::GetStaticMeshPaths())
	{
		FEditorAssetService::AddUniquePath(StaticMeshPaths, Path);
	}
	for (const FString& Path : FResourceManager::Get().GetStaticMeshPaths())
	{
		FEditorAssetService::AddUniquePath(StaticMeshPaths, Path);
	}

	ListAssetFiles(L"SkeletalMesh", { ".fbx" }, SkeletalMeshPaths);
	for (const FString& Path : FResourceManager::Get().GetSkeletalMeshPaths())
	{
		FEditorAssetService::AddUniquePath(SkeletalMeshPaths, Path);
	}

	for (const FString& Path : FAssetQueryService::GetTexturePaths())
	{
		FEditorAssetService::AddUniquePath(TexturePaths, Path);
	}
	for (const FString& Path : FResourceManager::Get().GetTextureFilePath())
	{
		FEditorAssetService::AddUniquePath(TexturePaths, Path);
	}

	for (const FString& Name : FResourceManager::Get().GetMaterialInterfaceNames())
	{
		FEditorAssetService::AddUniquePath(MaterialInterfaceNames, Name);
	}
	CachedMaterialInterfaces.resize(MaterialInterfaceNames.size(), nullptr);
	CachedMaterialInterfaceResolved.resize(MaterialInterfaceNames.size(), false);
	for (const FString& Name : FResourceManager::Get().GetFontNames())
	{
		FEditorAssetService::AddUniquePath(FontNames, Name);
	}
	for (const FString& Name : FResourceManager::Get().GetParticleNames())
	{
		FEditorAssetService::AddUniquePath(ParticleNames, Name);
	}

	BuildItems(StaticMeshPaths, EEditorAssetType::StaticMesh, StaticMeshItems);
	BuildItems(SkeletalMeshPaths, EEditorAssetType::SkeletalMesh, SkeletalMeshItems);
	BuildItems(TexturePaths, EEditorAssetType::Texture, TextureItems);
	BuildItems(MaterialInterfaceNames, EEditorAssetType::Material, MaterialItems);
	BuildItems(FontNames, EEditorAssetType::Font, FontItems);
	BuildItems(ParticleNames, EEditorAssetType::Particle, ParticleItems);
}

const TArray<FEditorAssetItem>& FEditorAssetService::GetAssets(EEditorAssetType Type) const
{
	switch (Type)
	{
	case EEditorAssetType::StaticMesh:
		return StaticMeshItems;
	case EEditorAssetType::SkeletalMesh:
		return SkeletalMeshItems;
	case EEditorAssetType::Texture:
		return TextureItems;
	case EEditorAssetType::Material:
		return MaterialItems;
	case EEditorAssetType::Font:
		return FontItems;
	case EEditorAssetType::Particle:
		return ParticleItems;
	case EEditorAssetType::Scene:
	case EEditorAssetType::Script:
	default:
		return EmptyItems;
	}
}

UStaticMesh* FEditorAssetService::LoadStaticMesh(const FString& Path) const
{
	return FResourceManager::Get().LoadStaticMesh(Path);
}

USkeletalMesh* FEditorAssetService::LoadSkeletalMesh(const FString& Path) const
{
	return FResourceManager::Get().LoadSkeletalMesh(Path);
}

UTexture* FEditorAssetService::LoadTexture(const FString& Path) const
{
	return FResourceManager::Get().LoadTexture(Path);
}

UMaterialInterface* FEditorAssetService::GetMaterialInterface(const FString& NameOrPath) const
{
	return FResourceManager::Get().GetMaterialInterface(NameOrPath);
}

UMaterialInterface* FEditorAssetService::ResolveMaterialInterface(const FString& NameOrPath)
{
	for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialInterfaceNames.size()); ++MaterialIndex)
	{
		if (MaterialInterfaceNames[MaterialIndex] == NameOrPath)
		{
			return ResolveMaterialInterfaceByIndex(MaterialIndex);
		}
	}

	return FResourceManager::Get().GetMaterialInterface(NameOrPath);
}

UMaterialInterface* FEditorAssetService::ResolveMaterialInterfaceByIndex(int32 MaterialIndex)
{
	if (MaterialIndex < 0 || MaterialIndex >= static_cast<int32>(MaterialInterfaceNames.size()))
	{
		return nullptr;
	}

	if (CachedMaterialInterfaceResolved[MaterialIndex])
	{
		return CachedMaterialInterfaces[MaterialIndex];
	}

	UMaterialInterface* Material = FResourceManager::Get().GetMaterialInterface(MaterialInterfaceNames[MaterialIndex]);
	CachedMaterialInterfaceResolved[MaterialIndex] = true;
	CachedMaterialInterfaces[MaterialIndex] = Material;
	return Material;
}

UTexture* FEditorAssetService::GetMaterialPreviewTexture(UMaterialInterface* Material) const
{
	if (!Material)
	{
		return nullptr;
	}

	FMaterialParamValue ParamValue;
	if (Material->GetParam("DiffuseMap", ParamValue)
		&& ParamValue.Type == EMaterialParamType::Texture
		&& std::holds_alternative<UTexture*>(ParamValue.Value))
	{
		if (UTexture* Texture = std::get<UTexture*>(ParamValue.Value))
		{
			if (Texture->GetSRV())
			{
				return Texture;
			}
		}
	}

	TMap<FString, FMaterialParamValue> Params;
	Material->GatherAllParams(Params);
	for (const auto& [ParamName, Param] : Params)
	{
		if (Param.Type != EMaterialParamType::Texture || !std::holds_alternative<UTexture*>(Param.Value))
		{
			continue;
		}

		if (UTexture* Texture = std::get<UTexture*>(Param.Value))
		{
			if (Texture->GetSRV())
			{
				return Texture;
			}
		}
	}

	return nullptr;
}

UMaterialInstance* FEditorAssetService::CreateMaterialInstance(const FString& InstancePath, UMaterial* Parent) const
{
	return FResourceManager::Get().CreateMaterialInstance(InstancePath, Parent);
}

bool FEditorAssetService::SaveMaterialInstance(const FString& InstancePath, UMaterialInstance* Instance) const
{
	return FResourceManager::Get().SerializeMaterialInstance(InstancePath, Instance);
}

void FEditorAssetService::AddUniquePath(TArray<FString>& Paths, const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!NormalizedPath.empty() && std::find(Paths.begin(), Paths.end(), NormalizedPath) == Paths.end())
	{
		Paths.push_back(NormalizedPath);
	}
}

void FEditorAssetService::BuildItems(const TArray<FString>& Paths, EEditorAssetType Type, TArray<FEditorAssetItem>& OutItems)
{
	OutItems.clear();
	OutItems.reserve(Paths.size());

	for (const FString& Path : Paths)
	{
		FEditorAssetItem Item;
		Item.Path = Path;
		Item.DisplayName = ToDisplayName(Path);
		Item.Type = Type;
		OutItems.push_back(std::move(Item));
	}
}
