#include "Particles/ParticleSystemManager.h"

#include "Asset/AssetPackage.h"
#include "Particles/ParticleSystem.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Object/ReferenceCollector.h"

#include <algorithm>
#include <filesystem>

UParticleSystem* FParticleSystemManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedParticleSystems.find(NormalizedPath);
	if (It != LoadedParticleSystems.end())
	{
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		return nullptr;
	}

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::ParticleSystem))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UParticleSystem* NewSystem = new UParticleSystem();
	NewSystem->Serialize(Ar);

	if (!Ar.IsValid())
	{
		delete NewSystem;
		return nullptr;
	}

	NewSystem->SetAssetPathFileName(NormalizedPath);
	LoadedParticleSystems.emplace(NormalizedPath, NewSystem);
	return NewSystem;
}

UParticleSystem* FParticleSystemManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedParticleSystems.find(NormalizedPath);
	return It != LoadedParticleSystems.end() ? It->second : nullptr;
}

void FParticleSystemManager::Register(const FString& Path, UParticleSystem* System)
{
	if (!System)
	{
		return;
	}

	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	System->SetAssetPathFileName(NormalizedPath);
	LoadedParticleSystems[NormalizedPath] = System;
}

bool FParticleSystemManager::Save(UParticleSystem* System)
{
	if (!System)
	{
		return false;
	}

	const FString& Path = System->GetAssetPathFileName();
	if (Path.empty() || Path == "None")
	{
		return false;
	}

	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid())
	{
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::ParticleSystem);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;
	System->Serialize(Ar);

	if (Ar.IsValid())
	{
		RefreshAvailableParticleSystems();
	}

	return Ar.IsValid();
}

void FParticleSystemManager::RefreshAvailableParticleSystems()
{
	namespace fs = std::filesystem;

	const fs::path ContentRoot = fs::path(FPaths::RootDir()) / L"Content";
	if (!fs::exists(ContentRoot))
	{
		return;
	}

	const fs::path ProjectRoot(FPaths::RootDir());
	AvailableParticleSystemFiles.clear();

	for (const auto& Entry : fs::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		std::wstring Ext = Entry.path().extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".uasset")
		{
			continue;
		}

		const FString RelPath =
			FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::ParticleSystem, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
		Item.FullPath = RelPath;
		AvailableParticleSystemFiles.push_back(std::move(Item));
	}
}

void FParticleSystemManager::Release()
{
	for (auto& Pair : LoadedParticleSystems)
	{
		delete Pair.second;
	}

	LoadedParticleSystems.clear();
	AvailableParticleSystemFiles.clear();
}

void FParticleSystemManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, System] : LoadedParticleSystems)
	{
		Collector.AddReferencedObject(System);
	}
}
