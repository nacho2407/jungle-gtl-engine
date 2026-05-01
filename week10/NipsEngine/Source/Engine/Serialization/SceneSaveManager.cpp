#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include <unordered_map>

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
	static constexpr const char* ActorClass         = "ActorClass";
	static constexpr const char* WorldType          = "WorldType";
	static constexpr const char* ContextName        = "ContextName";
	static constexpr const char* ContextHandle      = "ContextHandle";
	static constexpr const char* Actors             = "Actors";
	static constexpr const char* Visible            = "Visible";
	static constexpr const char* RootComponent      = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties         = "Properties";
	static constexpr const char* Children           = "Children";

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
	static constexpr const char* OwnerRootUUID      = "OwnerRootUUID"; // 비씬 컴포넌트가 속한 Actor의 루트 컴포넌트 UUID
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

	int32 Version = 4;
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

	Writer.BeginObject(SceneKeys::Primitives);
	for (AActor* Actor : WorldContext.World->GetPersistentLevel()->GetActors())
	{
		if (!Actor) continue;
		
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp) continue;
			if (Comp->IsTransient()) continue; // 직렬화가 꺼진 컴포넌트는 저장하지 않음
			
			Writer.BeginObject(std::to_string(Comp->GetUUID()));
			Comp->Serialize(Writer);
			if (Comp == Actor->GetRootComponent())
			{
				Writer << SceneKeys::ActorClass << Actor->GetTypeInfo()->name;
			}
			if (!Comp->IsA<USceneComponent>())
			{
				if (USceneComponent* OwnerRoot = Actor->GetRootComponent())
				{
					uint32 OwnerRootUUID = OwnerRoot->GetUUID();
					Writer << SceneKeys::OwnerRootUUID << OwnerRootUUID;
				}
			}
			Writer.EndObject();
		}
	}
	Writer.EndObject();

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

	json::JSON& PrimitivesNode = Root[SceneKeys::Primitives];
	TMap<uint32, UActorComponent*> UUIDToComp;
	TArray<uint32> RootUUIDs;

	// 1단계: 계층 구조 및 루트 컴포넌트 식별
	TMap<uint32, TArray<uint32>> ChildrenMap;
	TMap<uint32, uint32> NonSceneToRootMap;

	for (auto& [UUIDStr, Data] : PrimitivesNode.ObjectRange())
	{
		uint32 UUID = static_cast<uint32>(std::stoul(UUIDStr));
		if (Data.hasKey(SceneKeys::ParentUUID))
		{
			ChildrenMap[static_cast<uint32>(Data[SceneKeys::ParentUUID].ToInt())].push_back(UUID);
		}
		else if (Data.hasKey(SceneKeys::OwnerRootUUID))
		{
			NonSceneToRootMap[UUID] = static_cast<uint32>(Data[SceneKeys::OwnerRootUUID].ToInt());
		}
		else
		{
			RootUUIDs.push_back(UUID);
		}
	}

	auto GetNormalizedType = [](FString Type) -> FString {
		if (Type == "StaticMeshComp") return "UStaticMeshComponent";
		return Type;
	};

	auto InferActorClass = [&](json::JSON& RootCompData, const FString& CompType) -> FString
	{
		if (RootCompData.hasKey(SceneKeys::ActorClass))
		{
			return RootCompData[SceneKeys::ActorClass].ToString();
		}
		if (CompType == "StaticMeshComp" || CompType == "UStaticMeshComponent") return "AStaticMeshActor";
		if (CompType.length() > 10 && CompType.substr(CompType.size() - 9) == "Component")
		{
			FString BaseName = CompType.substr(0, CompType.size() - 9);
			if (BaseName[0] == 'U') BaseName = BaseName.substr(1);
			return "A" + BaseName + "Actor";
		}
		return "AActor";
	};

	// 재귀 매핑 함수: Actor의 기본 컴포넌트와 JSON의 컴포넌트를 UUID 및 타입으로 매칭
	std::function<void(AActor*, USceneComponent*, uint32)> MapSceneComp;
	MapSceneComp = [&](AActor* Actor, USceneComponent* ActorComp, uint32 JSONUUID)
	{
		ActorComp->SetUUID(JSONUUID);
		UUIDToComp[JSONUUID] = ActorComp;

		if (ChildrenMap.count(JSONUUID))
		{
			TArray<uint32>& JSONChildren = ChildrenMap[JSONUUID];
			TArray<USceneComponent*> ActorChildren = ActorComp->GetChildren();

			for (uint32 ChildUUID : JSONChildren)
			{
				FString ChildType = GetNormalizedType(PrimitivesNode[std::to_string(ChildUUID)][SceneKeys::Type].ToString());

				USceneComponent* Matched = nullptr;
				for (auto it = ActorChildren.begin(); it != ActorChildren.end(); ++it)
				{
					if ((*it)->GetTypeInfo()->name == ChildType)
					{
						Matched = *it;
						ActorChildren.erase(it);
						break;
					}
				}

				if (Matched)
				{
					MapSceneComp(Actor, Matched, ChildUUID);
				}
				else
				{
					UObject* NewObj = FObjectFactory::Get().Create(ChildType);
					USceneComponent* NewComp = Cast<USceneComponent>(NewObj);
					if (NewComp)
					{
						Actor->RegisterComponent(NewComp);
						NewComp->AttachToComponent(ActorComp);
						MapSceneComp(Actor, NewComp, ChildUUID);
					}
				}
			}
		}
	};

	// 2단계: Actor 생성 및 컴포넌트 매핑 (InitDefaultComponents 호출 후 UUID 매칭)
	for (uint32 RootUUID : RootUUIDs)
	{
		json::JSON& RootCompData = PrimitivesNode[std::to_string(RootUUID)];
		FString CompType = RootCompData[SceneKeys::Type].ToString();
		
		FString ClassName = InferActorClass(RootCompData, CompType);
		AActor* NewActor = Cast<AActor>(FObjectFactory::Get().Create(ClassName));
		if (NewActor)
		{
			NewActor->InitDefaultComponents();
			NewActor->SetWorld(World);
			if (ULevel* Level = World->GetPersistentLevel())
				Level->AddActor(NewActor);

			USceneComponent* RootComp = NewActor->GetRootComponent();
			if (RootComp)
			{
				MapSceneComp(NewActor, RootComp, RootUUID);
			}

			// Non-Scene 컴포넌트 매칭
			TArray<UActorComponent*> ActorComps = NewActor->GetComponents();
			for (auto& Pair : NonSceneToRootMap)
			{
				if (Pair.second == RootUUID)
				{
					uint32 CompUUID = Pair.first;
					FString Type = GetNormalizedType(PrimitivesNode[std::to_string(CompUUID)][SceneKeys::Type].ToString());
					
					UActorComponent* Matched = nullptr;
					for (auto it = ActorComps.begin(); it != ActorComps.end(); ++it)
					{
						if (!(*it)->IsA<USceneComponent>() && (*it)->GetTypeInfo()->name == Type)
						{
							Matched = *it;
							ActorComps.erase(it);
							break;
						}
					}

					if (Matched)
					{
						Matched->SetUUID(CompUUID);
						UUIDToComp[CompUUID] = Matched;
					}
					else
					{
						UActorComponent* NewComp = Cast<UActorComponent>(FObjectFactory::Get().Create(Type));
						if (NewComp)
						{
							NewActor->RegisterComponent(NewComp);
							NewComp->SetUUID(CompUUID);
							UUIDToComp[CompUUID] = NewComp;
						}
					}
				}
			}
		}
	}

	// 3단계: 매핑된 컴포넌트들의 데이터 역직렬화
	Reader.BeginObject(SceneKeys::Primitives);
	for (auto& Pair : UUIDToComp)
	{
		Reader.BeginObject(std::to_string(Pair.first));
		Pair.second->Serialize(Reader);
		Pair.second->PostEditProperty("Rotation");
		Reader.EndObject();
		
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Pair.second))
		{
			SceneComp->MarkTransformDirty();
		}
	}
	Reader.EndObject();

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
