#include "Serialization/PrefabManager.h"

#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Serialization/ActorSerialization.h"
#include "SimpleJSON/json.hpp"

#include <fstream>

namespace PrefabKeys
{
	static constexpr const char* ClassName = "ClassName";
	static constexpr const char* Version = "Version";
	static constexpr const char* SourceActorName = "SourceActorName";
	static constexpr const char* Actor = "Actor";
}

namespace
{
	bool IsPathInsideRoot(const std::filesystem::path& Path, const std::filesystem::path& Root)
	{
		const std::filesystem::path NormalPath = Path.lexically_normal();
		const std::filesystem::path NormalRoot = Root.lexically_normal();
		auto PathIt = NormalPath.begin();
		auto RootIt = NormalRoot.begin();
		for (; RootIt != NormalRoot.end(); ++RootIt, ++PathIt)
		{
			if (PathIt == NormalPath.end() || *PathIt != *RootIt)
			{
				return false;
			}
		}
		return true;
	}
}

bool FPrefabManager::ResolvePrefabPath(const FString& Path, std::filesystem::path& OutPath, bool bForSave)
{
	if (Path.empty())
	{
		return false;
	}

	std::filesystem::path Candidate(FPaths::ToWide(Path));
	if (!Candidate.is_absolute())
	{
		const std::wstring Generic = Candidate.generic_wstring();
		if (Generic.rfind(L"Asset/", 0) == 0 || Generic.rfind(L"Asset\\", 0) == 0)
		{
			Candidate = std::filesystem::path(FPaths::RootDir()) / Candidate;
		}
		else
		{
			Candidate = std::filesystem::path(GetPrefabDirectory()) / Candidate;
		}
	}

	if (Candidate.extension().empty())
	{
		Candidate += PrefabExtension;
	}

	Candidate = Candidate.lexically_normal();
	const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
	if (!IsPathInsideRoot(Candidate, AssetRoot))
	{
		return false;
	}

	if (bForSave)
	{
		std::error_code Ec;
		std::filesystem::create_directories(Candidate.parent_path(), Ec);
		if (Ec)
		{
			return false;
		}
	}

	OutPath = Candidate;
	return true;
}

FString FPrefabManager::MakeRelativePrefabPath(const std::filesystem::path& AbsolutePath)
{
	return FPaths::ToUtf8(FPaths::ToRelative(AbsolutePath.wstring()));
}

bool FPrefabManager::SaveActorPrefab(AActor* Actor, const FString& FilePath)
{
	if (!Actor)
	{
		return false;
	}

	std::filesystem::path PrefabPath;
	if (!ResolvePrefabPath(FilePath, PrefabPath, true))
	{
		UE_LOG("[Prefab] Invalid prefab save path: %s", FilePath.c_str());
		return false;
	}

	json::JSON Root = json::Object();
	Root[PrefabKeys::ClassName] = "NipsPrefab";
	Root[PrefabKeys::Version] = 1;
	Root[PrefabKeys::SourceActorName] = Actor->GetName();
	Root[PrefabKeys::Actor] = FActorSerialization::BuildActorJson(Actor);

	std::ofstream File(PrefabPath);
	if (!File.is_open())
	{
		UE_LOG("[Prefab] Failed to open prefab file: %s", FPaths::ToUtf8(PrefabPath.wstring()).c_str());
		return false;
	}

	File << Root.dump();
	File.flush();
	return true;
}

AActor* FPrefabManager::SpawnActorFromPrefab(UWorld* World, const FString& RelativePath)
{
	if (!World)
	{
		return nullptr;
	}

	std::filesystem::path PrefabPath;
	if (!ResolvePrefabPath(RelativePath, PrefabPath, false))
	{
		UE_LOG("[Prefab] Invalid prefab path: %s", RelativePath.c_str());
		return nullptr;
	}

	std::ifstream File(PrefabPath);
	if (!File.is_open())
	{
		UE_LOG("[Prefab] Failed to open prefab file: %s", FPaths::ToUtf8(PrefabPath.wstring()).c_str());
		return nullptr;
	}

	FString FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	json::JSON Root = json::JSON::Load(FileContent);
	if (!Root.hasKey(PrefabKeys::Actor))
	{
		UE_LOG("[Prefab] Prefab has no actor payload: %s", FPaths::ToUtf8(PrefabPath.wstring()).c_str());
		return nullptr;
	}

	FActorLoadOptions Options;
	Options.bPreserveUUIDs = false;
	Options.bPreserveName = true;
	Options.bMakeNameUnique = true;
	Options.bCallBeginPlayIfWorldBegunPlay = true;

	json::JSON& ActorData = Root[PrefabKeys::Actor];
	AActor* Actor = FActorSerialization::SpawnActorFromJson(World, ActorData, Options);
	if (Actor)
	{
		World->SyncSpatialIndex();
	}
	return Actor;
}
