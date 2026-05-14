#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <unordered_set>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/TextRenderComponent.h"
#include "Object/Object.h"
#include "Object/ActorIterator.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Resource/Material.h"
#include "Core/ResourceManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/ActorSerialization.h"
#include "Selection/SelectionManager.h"

namespace SceneKeys
{
	static constexpr const char* Version            = "Version";
	static constexpr const char* Name               = "Name";
	static constexpr const char* ClassName          = "ClassName";
	static constexpr const char* Tags               = "Tags";
	static constexpr const char* UUID               = "UUID";
	static constexpr const char* Components         = "Components";
	static constexpr const char* WorldType          = "WorldType";
	static constexpr const char* WorldSettings      = "WorldSettings";
	static constexpr const char* OverrideGameMode   = "OverrideGameMode";
	static constexpr const char* GameModeClass      = "GameModeClass";
	static constexpr const char* PlayerControllerClass = "PlayerControllerClass";
	static constexpr const char* DefaultPawnClass   = "DefaultPawnClass";
	static constexpr const char* DefaultPawnPrefabPath = "DefaultPawnPrefabPath";
	static constexpr const char* ContextName        = "ContextName";
	static constexpr const char* ContextHandle      = "ContextHandle";
	static constexpr const char* Actors             = "Actors";
	static constexpr const char* Visible            = "Visible";
	static constexpr const char* RootComponent      = "RootComponent";

	// PerspectiveCamera 섹션
	static constexpr const char* PerspectiveCamera  = "PerspectiveCamera";
	static constexpr const char* Primitives         = "Primitives";
	static constexpr const char* Scale              = "Scale";
	static constexpr const char* Location           = "Location";
	static constexpr const char* Rotation           = "Rotation";
	static constexpr const char* FOV                = "FOV";
	static constexpr const char* NearClip           = "NearClip";
	static constexpr const char* FarClip            = "FarClip";
	static constexpr const char* Type               = "Type";
	static constexpr const char* NextUUID           = "NextUUID";
	static constexpr const char* ParentUUID         = "ParentUUID";
}

namespace
{
	FString GetJsonString(json::JSON& Object, const char* Key, const FString& DefaultValue = "")
	{
		return Object.hasKey(Key) ? Object[Key].ToString() : DefaultValue;
	}

	uint32 GetJsonUInt(json::JSON& Object, const char* Key, uint32 DefaultValue = 0)
	{
		return Object.hasKey(Key) ? static_cast<uint32>(Object[Key].ToInt()) : DefaultValue;
	}

	int32 ParseJsonKeyAsInt(const FString& Key)
	{
		try
		{
			return std::stoi(Key);
		}
		catch (...)
		{
			return 0;
		}
	}

	void CollectLegacyComponentIds(
		const FString& RootUUID,
		const TMap<FString, TArray<FString>>& ChildrenByParent,
		TArray<FString>& OutIds)
	{
		OutIds.push_back(RootUUID);

		auto It = ChildrenByParent.find(RootUUID);
		if (It == ChildrenByParent.end())
		{
			return;
		}

		for (const FString& ChildUUID : It->second)
		{
			CollectLegacyComponentIds(ChildUUID, ChildrenByParent, OutIds);
		}
	}

	json::JSON ConvertLegacyPrimitivesToActors(json::JSON& PrimitivesNode)
	{
		json::JSON Actors = json::Array();
		TArray<FString> RootUUIDs;
		TMap<FString, TArray<FString>> ChildrenByParent;

		for (auto& Entry : PrimitivesNode.ObjectRange())
		{
			json::JSON& ComponentData = Entry.second;
			if (ComponentData.hasKey(SceneKeys::ParentUUID))
			{
				const FString ParentUUID = std::to_string(GetJsonUInt(ComponentData, SceneKeys::ParentUUID));
				ChildrenByParent[ParentUUID].push_back(Entry.first);
			}
			else
			{
				RootUUIDs.push_back(Entry.first);
			}
		}

		std::sort(
			RootUUIDs.begin(),
			RootUUIDs.end(),
			[](const FString& A, const FString& B)
			{
				return ParseJsonKeyAsInt(A) < ParseJsonKeyAsInt(B);
			});

		int32 ActorIndex = 1;
		for (const FString& RootUUID : RootUUIDs)
		{
			json::JSON& RootComponentData = PrimitivesNode[RootUUID];
			TArray<FString> ComponentIds;
			CollectLegacyComponentIds(RootUUID, ChildrenByParent, ComponentIds);

			FString ActorClass = GetJsonString(RootComponentData, "ActorClass", "ASceneActor");
			for (const FString& ComponentId : ComponentIds)
			{
				json::JSON& ComponentData = PrimitivesNode[ComponentId];
				if (ComponentData.hasKey("ActorClass"))
				{
					ActorClass = ComponentData["ActorClass"].ToString();
					break;
				}
			}

			json::JSON ActorJson = json::Object();
			const uint32 RootComponentUUID = static_cast<uint32>(ParseJsonKeyAsInt(RootUUID));
			ActorJson[SceneKeys::UUID] = static_cast<int32>(RootComponentUUID);
			ActorJson[SceneKeys::ClassName] = ActorClass;
			ActorJson[SceneKeys::Name] = ActorClass + "_" + std::to_string(ActorIndex++);
			ActorJson[SceneKeys::Visible] = RootComponentData.hasKey(SceneKeys::Visible) ? RootComponentData[SceneKeys::Visible].ToBool() : true;
			ActorJson["EditorOnly"] = RootComponentData.hasKey("Editor Only") ? RootComponentData["Editor Only"].ToBool() : false;
			ActorJson[SceneKeys::RootComponent] = static_cast<int32>(RootComponentUUID);
			ActorJson[SceneKeys::Tags] = json::Array();
			ActorJson[SceneKeys::Components] = json::Array();

			for (const FString& ComponentId : ComponentIds)
			{
				json::JSON ComponentJson = PrimitivesNode[ComponentId];
				const uint32 ComponentUUID = static_cast<uint32>(ParseJsonKeyAsInt(ComponentId));
				ComponentJson[SceneKeys::UUID] = static_cast<int32>(ComponentUUID);
				ComponentJson[SceneKeys::ClassName] = GetJsonString(ComponentJson, SceneKeys::ClassName, GetJsonString(ComponentJson, SceneKeys::Type, "USceneComponent"));
				ActorJson[SceneKeys::Components].append(ComponentJson);
			}

			Actors.append(ActorJson);
		}

		return Actors;
	}

	void EnsureUniqueComponentPersistentGuids(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		std::unordered_set<FString> UsedGuids;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component)
				{
					continue;
				}

				Component->EnsurePersistentGuid();
				FString GuidText = Component->GetPersistentGuid().ToString();
				while (GuidText.empty() || UsedGuids.find(GuidText) != UsedGuids.end())
				{
					Component->RegeneratePersistentGuid();
					GuidText = Component->GetPersistentGuid().ToString();
				}

				UsedGuids.insert(GuidText);
			}
		}
	}
}

static const char* WorldTypeToString(EWorldType Type)
{
	switch (Type) {
	case EWorldType::Game: return "Game";
	case EWorldType::PIE:  return "PIE";
	default:               return "Editor";
	}
}

static EWorldType StringToWorldType(const FString& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

static json::JSON BuildSceneSnapshotJson(const FString& SceneName, FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	json::JSON Root = json::Object();
	FJsonWriter Writer(Root);

	FString FinalName = SceneName.empty() ? "Snapshot" : SceneName;

	int32 Version = 6;
	uint32 NextUUID = EngineStatics::GetNextUUID();

	Writer << SceneKeys::ClassName << WorldContext.World->GetTypeInfo()->name;
	Writer << SceneKeys::Name << FinalName;
	Writer << SceneKeys::WorldType << WorldTypeToString(WorldContext.WorldType);
	Writer << SceneKeys::Version << Version;
	Writer << SceneKeys::NextUUID << NextUUID;

	const FWorldGameModeSettings& GameModeSettings = WorldContext.World->GetGameModeSettings();
	Root[SceneKeys::WorldSettings] = json::Object();
	Root[SceneKeys::WorldSettings][SceneKeys::OverrideGameMode] = GameModeSettings.bOverrideGameMode;
	Root[SceneKeys::WorldSettings][SceneKeys::GameModeClass] = GameModeSettings.GameModeClass;
	Root[SceneKeys::WorldSettings][SceneKeys::PlayerControllerClass] = GameModeSettings.PlayerControllerClass;
	Root[SceneKeys::WorldSettings][SceneKeys::DefaultPawnClass] = GameModeSettings.DefaultPawnClass;
	Root[SceneKeys::WorldSettings][SceneKeys::DefaultPawnPrefabPath] = GameModeSettings.DefaultPawnPrefabPath;

	FEditorCameraState* CamState = const_cast<FEditorCameraState*>(CameraState);
	FVector CamRotation = CamState ? CamState->Rotation.Euler() : FVector::ZeroVector;

	if (CameraState && CameraState->bValid)
	{
		Writer.BeginObject(SceneKeys::PerspectiveCamera);
		Writer << SceneKeys::Location << CamState->Location;
		Writer << SceneKeys::Rotation << CamRotation;
		Writer << SceneKeys::FarClip << CamState->FarClip;
		Writer << SceneKeys::NearClip << CamState->NearClip;
		Writer << SceneKeys::FOV << CamState->FOV;
		Writer.EndObject();
	}

	Root[SceneKeys::Actors] = json::Array();
	for (AActor* Actor : WorldContext.World->GetPersistentLevel()->GetActors())
	{
		if (!Actor) continue;

		Root[SceneKeys::Actors].append(FActorSerialization::BuildActorJson(Actor));
	}

	return Root;
}

void FSceneSaveManager::Save(const FString& FilePath, FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	FString FinalName = FilePath.empty() ? "Save_" + GetCurrentTimeStamp() : FilePath;
	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	std::filesystem::create_directories(SceneDir);

	json::JSON Root = BuildSceneSnapshotJson(FinalName, WorldContext, CameraState);

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

bool FSceneSaveManager::SaveToFilePath(const FString& FilePath, FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	if (!WorldContext.World || FilePath.empty())
	{
		return false;
	}

	std::filesystem::path TargetPath(FPaths::ToWide(FilePath));
	const bool bNameOnlySave = !TargetPath.has_parent_path() && TargetPath.root_path().empty();
	if (TargetPath.extension().empty())
	{
		TargetPath += SceneExtension;
	}
	if (bNameOnlySave)
	{
		TargetPath = std::filesystem::path(GetSceneDirectory()) / TargetPath.filename();
	}

	const std::filesystem::path ParentPath = TargetPath.parent_path();
	if (!ParentPath.empty())
	{
		std::error_code CreateDirEc;
		std::filesystem::create_directories(ParentPath, CreateDirEc);
		if (CreateDirEc)
		{
			return false;
		}
	}

	const FString SceneName = FPaths::ToUtf8(TargetPath.stem().wstring());
	json::JSON Root = BuildSceneSnapshotJson(SceneName, WorldContext, CameraState);

	std::ofstream File(TargetPath, std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		return false;
	}

	File << Root.dump();
	File.flush();
	return true;
}

FString FSceneSaveManager::SaveToString(FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	if (!WorldContext.World)
	{
		return "";
	}

	json::JSON Root = BuildSceneSnapshotJson("UndoSnapshot", WorldContext, CameraState);
	return Root.dump();
}

void FSceneSaveManager::LoadFromString(const FString& Snapshot, FWorldContext& OutWorldContext, FEditorCameraState* OutCameraState)
{
	if (Snapshot.empty())
	{
		return;
	}

	std::filesystem::path TempPath = std::filesystem::temp_directory_path()
		/ (L"JSEngine_UndoRedo_" + FPaths::ToWide(GetCurrentTimeStamp()) + L".Scene");

	{
		std::ofstream TempFile(TempPath);
		if (!TempFile.is_open())
		{
			return;
		}
		TempFile << Snapshot;
	}

	Load(FPaths::ToUtf8(TempPath.wstring()), OutWorldContext, OutCameraState);

	std::error_code Ec;
	std::filesystem::remove(TempPath, Ec);
}

void FSceneSaveManager::Load(const FString& FilePath, FWorldContext& OutWorldContext, FEditorCameraState* OutCameraState)
{
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)));
	if (!File.is_open()) return;

	FString FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	json::JSON Root = json::JSON::Load(FileContent);
	FJsonReader Reader(Root);

	FString ClassName = Root.hasKey(SceneKeys::ClassName) ? Root[SceneKeys::ClassName].ToString() : "UWorld";
	UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);
	EWorldType WorldType = Root.hasKey(SceneKeys::WorldType) ? StringToWorldType(Root[SceneKeys::WorldType].ToString()) : EWorldType::Editor;
	if (Root.hasKey(SceneKeys::WorldSettings))
	{
		json::JSON& WorldSettingsNode = Root[SceneKeys::WorldSettings];
		FWorldGameModeSettings GameModeSettings;
		GameModeSettings.bOverrideGameMode = WorldSettingsNode.hasKey(SceneKeys::OverrideGameMode)
			? WorldSettingsNode[SceneKeys::OverrideGameMode].ToBool()
			: false;
		GameModeSettings.GameModeClass = GetJsonString(
			WorldSettingsNode,
			SceneKeys::GameModeClass,
			GameModeSettings.GameModeClass);
		GameModeSettings.PlayerControllerClass = GetJsonString(
			WorldSettingsNode,
			SceneKeys::PlayerControllerClass,
			GameModeSettings.PlayerControllerClass);
		GameModeSettings.DefaultPawnClass = GetJsonString(
			WorldSettingsNode,
			SceneKeys::DefaultPawnClass,
			GameModeSettings.DefaultPawnClass);
		GameModeSettings.DefaultPawnPrefabPath = FPaths::Normalize(GetJsonString(
			WorldSettingsNode,
			SceneKeys::DefaultPawnPrefabPath,
			GameModeSettings.DefaultPawnPrefabPath));
		World->SetGameModeSettings(GameModeSettings);
	}

	// UUID 카운터 복원
	if (Root.hasKey(SceneKeys::NextUUID))
		EngineStatics::ResetUUIDGeneration(Root[SceneKeys::NextUUID].ToInt());

	// Perspective 카메라 상태 복원
	if (OutCameraState)
	{
		OutCameraState->bValid = false;
		if (Root.hasKey(SceneKeys::PerspectiveCamera))
		{
			FVector CamRotation = FVector::ZeroVector;

			Reader.BeginObject(SceneKeys::PerspectiveCamera);
			Reader << SceneKeys::Location << OutCameraState->Location;
			Reader << SceneKeys::Rotation << CamRotation;
			Reader << SceneKeys::FarClip << OutCameraState->FarClip;
			Reader << SceneKeys::NearClip << OutCameraState->NearClip;
			Reader << SceneKeys::FOV << OutCameraState->FOV;
			Reader.EndObject();

			OutCameraState->Rotation = FRotator::MakeFromEuler(CamRotation);
			OutCameraState->bValid = true;
		}
	}

	if (!Root.hasKey(SceneKeys::Actors) && Root.hasKey(SceneKeys::Primitives))
	{
		Root[SceneKeys::Actors] = ConvertLegacyPrimitivesToActors(Root[SceneKeys::Primitives]);
	}

	if (Root.hasKey(SceneKeys::Actors))
	{
		json::JSON& ActorsNode = Root[SceneKeys::Actors];
		for (int32 ActorIndex = 0; ActorIndex < static_cast<int32>(ActorsNode.length()); ++ActorIndex)
		{
			json::JSON& ActorData = ActorsNode.at(ActorIndex);
			FActorLoadOptions Options;
			Options.bPreserveUUIDs = true;
			Options.bPreserveName = true;
			Options.bMakeNameUnique = false;
			Options.bCallBeginPlayIfWorldBegunPlay = false;
			FActorSerialization::SpawnActorFromJson(World, ActorData, Options);
		}
	}

	if (World)
	{
		EnsureUniqueComponentPersistentGuids(World);
		World->SyncSpatialIndex();
	}

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
    OutWorldContext.SelectionManager = new FSelectionManager;
    OutWorldContext.SelectionManager->Init();
}

FString FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}
