#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>

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

namespace SceneKeys
{
	static constexpr const char* Version            = "Version";
	static constexpr const char* Name               = "Name";
	static constexpr const char* ClassName          = "ClassName";
	static constexpr const char* Tags               = "Tags";
	static constexpr const char* UUID               = "UUID";
	static constexpr const char* Components         = "Components";
	static constexpr const char* WorldType          = "WorldType";
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

static FString GetNormalizedType(FString Type)
{
	if (Type == "StaticMeshComp")
	{
		return "UStaticMeshComponent";
	}
	return Type;
}

static FString GetJsonString(json::JSON& Object, const char* Key, const FString& DefaultValue = "")
{
	return Object.hasKey(Key) ? Object[Key].ToString() : DefaultValue;
}

static uint32 GetJsonUInt(json::JSON& Object, const char* Key, uint32 DefaultValue = 0)
{
	return Object.hasKey(Key) ? static_cast<uint32>(Object[Key].ToInt()) : DefaultValue;
}

static json::JSON BuildComponentJson(UActorComponent* Component)
{
	json::JSON ComponentJson = json::Object();
	FJsonWriter ComponentWriter(ComponentJson);
	Component->Serialize(ComponentWriter);

	ComponentJson[SceneKeys::UUID] = static_cast<int32>(Component->GetUUID());
	ComponentJson[SceneKeys::ClassName] = Component->GetTypeInfo()->name;
	return ComponentJson;
}

static json::JSON BuildActorJson(AActor* Actor)
{
	json::JSON ActorJson = json::Object();
	ActorJson[SceneKeys::UUID] = static_cast<int32>(Actor->GetUUID());
	ActorJson[SceneKeys::ClassName] = Actor->GetTypeInfo()->name;
	ActorJson[SceneKeys::Name] = Actor->GetName();
	ActorJson[SceneKeys::Visible] = Actor->IsVisible();
	ActorJson["EditorOnly"] = Actor->ShouldTickInEditor();
	ActorJson[SceneKeys::RootComponent] = Actor->GetRootComponent()
		? static_cast<int32>(Actor->GetRootComponent()->GetUUID())
		: 0;

	FJsonWriter ActorWriter(ActorJson);
	TArray<FString> ActorTags = Actor->GetTags();
	ActorWriter << SceneKeys::Tags << ActorTags;

	ActorJson[SceneKeys::Components] = json::Array();
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (!Component || Component->IsTransient())
		{
			continue;
		}

		ActorJson[SceneKeys::Components].append(BuildComponentJson(Component));
	}

	return ActorJson;
}

static json::JSON BuildSceneSnapshotJson(const FString& SceneName, FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	json::JSON Root = json::Object();
	FJsonWriter Writer(Root);

	FString FinalName = SceneName.empty() ? "Snapshot" : SceneName;

	int32 Version = 5;
	uint32 NextUUID = EngineStatics::GetNextUUID();

	Writer << SceneKeys::ClassName << WorldContext.World->GetTypeInfo()->name;
	Writer << SceneKeys::Name << FinalName;
	Writer << SceneKeys::WorldType << WorldTypeToString(WorldContext.WorldType);
	Writer << SceneKeys::Version << Version;
	Writer << SceneKeys::NextUUID << NextUUID;

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

		Root[SceneKeys::Actors].append(BuildActorJson(Actor));
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
		/ (L"NipsEngine_UndoRedo_" + FPaths::ToWide(GetCurrentTimeStamp()) + L".Scene");

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

	if (Root.hasKey(SceneKeys::Actors))
	{
		json::JSON& ActorsNode = Root[SceneKeys::Actors];
		for (int32 ActorIndex = 0; ActorIndex < static_cast<int32>(ActorsNode.length()); ++ActorIndex)
		{
			json::JSON& ActorData = ActorsNode.at(ActorIndex);
			const FString ActorClass = GetJsonString(ActorData, SceneKeys::ClassName, "AActor");
			AActor* NewActor = Cast<AActor>(FObjectFactory::Get().Create(ActorClass));
			if (!NewActor)
			{
				continue;
			}

			NewActor->InitDefaultComponents();
			NewActor->SetUUID(GetJsonUInt(ActorData, SceneKeys::UUID, NewActor->GetUUID()));
			const FString ActorName = GetJsonString(ActorData, SceneKeys::Name);
			if (!ActorName.empty())
			{
				NewActor->SetFName(FName(ActorName));
			}
			if (ActorData.hasKey(SceneKeys::Visible))
			{
				NewActor->SetVisible(ActorData[SceneKeys::Visible].ToBool());
			}
			if (ActorData.hasKey("EditorOnly"))
			{
				NewActor->SetTickInEditor(ActorData["EditorOnly"].ToBool());
			}
			if (ActorData.hasKey(SceneKeys::Tags))
			{
				TArray<FString> ActorTags;
				FJsonReader ActorReader(ActorData);
				ActorReader << SceneKeys::Tags << ActorTags;
				NewActor->ClearTags();
				for (const FString& Tag : ActorTags)
				{
					NewActor->AddTag(Tag);
				}
			}

			NewActor->SetWorld(World);
			if (ULevel* Level = World->GetPersistentLevel())
			{
				Level->AddActor(NewActor);
			}

			json::JSON& ComponentsNode = ActorData[SceneKeys::Components];
			const uint32 RootUUID = GetJsonUInt(ActorData, SceneKeys::RootComponent);
			TMap<uint32, UActorComponent*> UUIDToComp;
			TArray<UActorComponent*> UnusedDefaultComponents = NewActor->GetComponents();

			auto TakeDefaultComponent = [&](const FString& TypeName) -> UActorComponent*
			{
				for (auto It = UnusedDefaultComponents.begin(); It != UnusedDefaultComponents.end(); ++It)
				{
					UActorComponent* Candidate = *It;
					if (Candidate && GetNormalizedType(Candidate->GetTypeInfo()->name) == TypeName)
					{
						UnusedDefaultComponents.erase(It);
						return Candidate;
					}
				}
				return nullptr;
			};

			auto MarkDefaultComponentUsed = [&](UActorComponent* Component)
			{
				auto It = std::find(UnusedDefaultComponents.begin(), UnusedDefaultComponents.end(), Component);
				if (It != UnusedDefaultComponents.end())
				{
					UnusedDefaultComponents.erase(It);
				}
			};

			for (int32 CompIndex = 0; CompIndex < static_cast<int32>(ComponentsNode.length()); ++CompIndex)
			{
				json::JSON& CompData = ComponentsNode.at(CompIndex);
				const uint32 CompUUID = GetJsonUInt(CompData, SceneKeys::UUID);
				const FString Type = GetNormalizedType(GetJsonString(CompData, SceneKeys::ClassName, GetJsonString(CompData, SceneKeys::Type)));
				if (CompUUID == 0 || Type.empty())
				{
					continue;
				}

				UActorComponent* Component = nullptr;
				if (CompUUID == RootUUID && NewActor->GetRootComponent()
					&& GetNormalizedType(NewActor->GetRootComponent()->GetTypeInfo()->name) == Type)
				{
					Component = NewActor->GetRootComponent();
					MarkDefaultComponentUsed(Component);
				}
				if (!Component)
				{
					Component = TakeDefaultComponent(Type);
				}
				if (!Component)
				{
					UObject* NewObj = FObjectFactory::Get().Create(Type);
					Component = Cast<UActorComponent>(NewObj);
					if (!Component)
					{
						UObjectManager::Get().DestroyObject(NewObj);
						continue;
					}
					NewActor->RegisterComponent(Component);
				}

				Component->SetUUID(CompUUID);
				UUIDToComp[CompUUID] = Component;
				if (CompUUID == RootUUID)
				{
					if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
					{
						NewActor->SetRootComponent(SceneComponent);
					}
				}
			}

			for (int32 CompIndex = 0; CompIndex < static_cast<int32>(ComponentsNode.length()); ++CompIndex)
			{
				json::JSON& CompData = ComponentsNode.at(CompIndex);
				const uint32 CompUUID = GetJsonUInt(CompData, SceneKeys::UUID);
				const uint32 ParentUUID = GetJsonUInt(CompData, SceneKeys::ParentUUID);
				if (ParentUUID == 0)
				{
					continue;
				}

				USceneComponent* SceneComponent = Cast<USceneComponent>(UUIDToComp[CompUUID]);
				USceneComponent* ParentComponent = Cast<USceneComponent>(UUIDToComp[ParentUUID]);
				if (SceneComponent && ParentComponent)
				{
					SceneComponent->AttachToComponent(ParentComponent);
				}
			}

			for (int32 CompIndex = 0; CompIndex < static_cast<int32>(ComponentsNode.length()); ++CompIndex)
			{
				json::JSON& CompData = ComponentsNode.at(CompIndex);
				const uint32 CompUUID = GetJsonUInt(CompData, SceneKeys::UUID);
				UActorComponent* Component = UUIDToComp[CompUUID];
				if (!Component)
				{
					continue;
				}

				FJsonReader ComponentReader(CompData);
				Component->Serialize(ComponentReader);
				Component->PostEditProperty("Rotation");
				if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
				{
					SceneComponent->MarkTransformDirty();
				}
			}
		}
	}

	if (World)
		World->SyncSpatialIndex();

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
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
