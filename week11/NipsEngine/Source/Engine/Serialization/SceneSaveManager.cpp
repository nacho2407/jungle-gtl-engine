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

static EWorldType StringToWorldType(const string& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext,
                                        const FEditorCameraState* CameraState)
{
    using namespace json;
    if (!WorldContext.World) return;

    string FinalName = InSceneName.empty() ? "Save_" + GetCurrentTimeStamp() : InSceneName;
    std::wstring SceneDir = GetSceneDirectory();
    std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
    std::filesystem::create_directories(SceneDir);

    JSON Root = json::Object();
    Root[SceneKeys::Version] = 4;
    Root[SceneKeys::Name] = FinalName;
    Root[SceneKeys::ClassName] = WorldContext.World->GetTypeInfo()->name;
    Root[SceneKeys::WorldType] = WorldTypeToString(WorldContext.WorldType);
    Root[SceneKeys::PerspectiveCamera] = SerializeCameraState(CameraState);
    Root[SceneKeys::Primitives] = SerializeWorldToPrimitives(WorldContext.World, WorldContext);
    Root[SceneKeys::NextUUID] = static_cast<int>(EngineStatics::GetNextUUID());

    std::ofstream File(FileDestination);
    if (File.is_open()) {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeWorldToPrimitives(UWorld* World, const FWorldContext& Ctx)
{
    json::JSON Primitives = json::Object();
    if (ULevel* PersistentLevel = World->GetPersistentLevel())
    {
        for (AActor* Actor : PersistentLevel->GetActors())
        {
            if (!Actor) continue;
            if (USceneComponent* RootComp = Actor->GetRootComponent())
            {
                CollectComponentsFlat(RootComp, 0, Primitives);
                //Primitives[std::to_string(RootComp->GetUUID())]["ActorClass"] = Actor->GetTypeInfo()->name;
                CollectNonSceneComponents(Actor, Primitives);
            }
        }
    }
    return Primitives;
}

// 재귀 함수: Comp 및 모든 자손 컴포넌트를 OutPrimitives 에 평탄하게 수집
// ParentID == 0 은 부모 없음(루트 컴포넌트)을 의미 (UUID는 1부터 시작)
void FSceneSaveManager::CollectComponentsFlat(USceneComponent* Comp, uint32 ParentID, json::JSON& OutPrimitives)
{
	if (Comp->IsTransient()) return;

    json::JSON PrimObj = json::Object();

    // 타입 이름 매핑
    FString ClassName = Comp->GetTypeInfo()->name;
    if (ClassName == "UStaticMeshComponent") ClassName = "StaticMeshComp";
    PrimObj[SceneKeys::Type] = ClassName;

    // 루트가 아닌 경우에만 ParentUUID 기록
    if (ParentID != 0)
        PrimObj[SceneKeys::ParentUUID] = static_cast<int>(ParentID);

    // 프로퍼티 기반 직렬화
    TArray<FPropertyDescriptor> Descriptors;
    Comp->GetEditableProperties(Descriptors);
    for (const auto& Prop : Descriptors)
    {
        FString OutKey = Prop.Name;
        if (strcmp(Prop.Name, "StaticMesh") == 0) OutKey = "ObjStaticMeshAsset";
        PrimObj[OutKey] = SerializePropertyValue(Prop);
    }

    const uint32 MyUUID = Comp->GetUUID();
    OutPrimitives[std::to_string(MyUUID)] = PrimObj;

    // 자식 컴포넌트 재귀 수집
    for (USceneComponent* Child : Comp->GetChildren())
        CollectComponentsFlat(Child, MyUUID, OutPrimitives);
}

// Actor가 소유한 비씬 ActorComponent(MovementComponent 등)를 Primitives 맵에 직렬화한다.
// OwnerRootUUID를 기록해 역직렬화 시 대응하는 Actor를 찾을 수 있도록 한다.
void FSceneSaveManager::CollectNonSceneComponents(AActor* Actor, json::JSON& OutPrimitives)
{
    USceneComponent* RootComp = Actor->GetRootComponent();
    if (!RootComp) return;

    const uint32 RootUUID = RootComp->GetUUID();

    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp) continue;
        if (Comp->IsA<USceneComponent>()) continue; // SceneComponent 트리는 이미 처리됨
		if (Comp->IsTransient()) continue; // 직렬화가 꺼진 컴포넌트는 저장하지 않음

        json::JSON CompObj = json::Object();
        CompObj[SceneKeys::Type] = Comp->GetTypeInfo()->name;
        CompObj[SceneKeys::OwnerRootUUID] = static_cast<int>(RootUUID);

        TArray<FPropertyDescriptor> Descriptors;
        Comp->GetEditableProperties(Descriptors);
        for (const auto& Prop : Descriptors)
            CompObj[Prop.Name] = SerializePropertyValue(Prop);

        OutPrimitives[std::to_string(Comp->GetUUID())] = CompObj;
    }
}

/* @brief 현재 사용하지 않는 함수, 추후 Actor-Component 단위로 계층화를 시켜야 한다면 이쪽을 사용 */
json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetTypeInfo()->name;
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	JSON Actors = json::Array();
	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (!Actor) continue;
		Actors.append(SerializeActor(Actor));
	}
	w[SceneKeys::Actors] = Actors;
	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetTypeInfo()->name;
	a[SceneKeys::Visible] = Actor->IsVisible();

	// 자식 컴포넌트 및 NonScene 컴포넌트는 무시하고 RootComponent만 직렬화
	if (Actor->GetRootComponent()) 
	{
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent());
	}

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	
	FString ClassName = Comp->GetTypeInfo()->name;
	if (ClassName == "UStaticMeshComponent") { ClassName = "StaticMeshComp"; }
	c[SceneKeys::Type] = ClassName;
	
	c[SceneKeys::Properties] = SerializeProperties(Comp);

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
	using namespace json;
	JSON props = json::Object();

	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (const auto& Prop : Descriptors) {
		props[Prop.Name] = SerializePropertyValue(Prop);
	}
	return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
	using namespace json;

	switch (Prop.Type) {
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(Prop.ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
    case EPropertyType::Color: {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        return arr;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
		return JSON(*static_cast<FString*>(Prop.ValuePtr));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	case EPropertyType::SceneComponentRef: {
		// USceneComponent* 포인터를 UUID로 직렬화 (-1은 nullptr)
		USceneComponent* RefComp = *static_cast<USceneComponent**>(Prop.ValuePtr);
		return RefComp ? JSON(static_cast<int>(RefComp->GetUUID())) : JSON(-1);
	}

	case EPropertyType::Vec3Array: {
		const auto& Arr = *static_cast<const TArray<FVector>*>(Prop.ValuePtr);
		JSON arr = json::Array();
		for (const FVector& v : Arr) {
			JSON elem = json::Array();
			elem.append(static_cast<double>(v.X));
			elem.append(static_cast<double>(v.Y));
			elem.append(static_cast<double>(v.Z));
			arr.append(elem);
		}
		return arr;
	}

	case EPropertyType::Enum: {
		return JSON(*static_cast<int32*>(Prop.ValuePtr));
	}

	case EPropertyType::Material: {
		const auto& Arr = *static_cast<const TArray<UMaterialInterface*>*>(Prop.ValuePtr);
		JSON arr = json::Array();
		for (const UMaterialInterface* s : Arr) {
			arr.append(s->GetName());
		}
		return arr;
	}

	default:
		return JSON();
	}
}

json::JSON FSceneSaveManager::SerializeCameraState(const FEditorCameraState* CameraState /*= nullptr*/)
{
	using namespace json;

	// Perspective 카메라 상태 저장
	if (CameraState && CameraState->bValid)
	{
		JSON Cam = Object();
		Cam[SceneKeys::Location] = Array(
			static_cast<double>(CameraState->Location.X),
			static_cast<double>(CameraState->Location.Y),
			static_cast<double>(CameraState->Location.Z));
		Cam[SceneKeys::Rotation] = Array(
			static_cast<double>(CameraState->Rotation.Pitch),
			static_cast<double>(CameraState->Rotation.Yaw),
			static_cast<double>(CameraState->Rotation.Roll));
		
		Cam[SceneKeys::FOV] = Array(static_cast<double>(CameraState->FOV));
		Cam[SceneKeys::NearClip] = Array(static_cast<double>(CameraState->NearClip));
		Cam[SceneKeys::FarClip] = Array(static_cast<double>(CameraState->FarClip));
		
		return Cam;
	}
	return nullptr;
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FEditorCameraState* OutCameraState)
{
    using json::JSON;
    std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
    if (!File.is_open()) return;

    string FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    JSON root = JSON::Load(FileContent);

    string ClassName = root.hasKey(SceneKeys::ClassName) ? root[SceneKeys::ClassName].ToString() : "UWorld";
    UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
    if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

    UWorld* World = static_cast<UWorld*>(WorldObj);
    EWorldType WorldType = root.hasKey(SceneKeys::WorldType) ? StringToWorldType(root[SceneKeys::WorldType].ToString()) : EWorldType::Editor;

    DeserializeCameraState(root, OutCameraState);

    // Primitives 파싱 (평탄화 포맷)
    if (root.hasKey(SceneKeys::Primitives))
        DeserializePrimitivesToWorld(root[SceneKeys::Primitives], World);

    // UUID 카운터 복원 — 저장 시점의 NextUUID 이후 값부터 새 오브젝트에 할당
    if (root.hasKey(SceneKeys::NextUUID))
        EngineStatics::ResetUUIDGeneration(root[SceneKeys::NextUUID].ToInt());

    OutWorldContext.WorldType = WorldType;
    OutWorldContext.World = World;
}

void FSceneSaveManager::Save(const FString& FilePath, FWorldContext& WorldContext, const FEditorCameraState* CameraState)
{
	json::JSON Root = json::Object();
	FJsonWriter Writer(Root);

	string FinalName = FilePath.empty() ? "Save_" + GetCurrentTimeStamp() : FilePath;
	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	std::filesystem::create_directories(SceneDir);

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
			Writer.EndObject();
		}
	}
	Writer.EndObject();


	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

void FSceneSaveManager::Load(const FString& FilePath, FWorldContext& OutWorldContext, FEditorCameraState* OutCameraState)
{
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)));
	if (!File.is_open()) return;

	string FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	json::JSON Root = json::JSON::Load(FileContent);
	FJsonReader Reader(Root);

	string ClassName = Root.hasKey(SceneKeys::ClassName) ? Root[SceneKeys::ClassName].ToString() : "UWorld";
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
		FVector CamRotation;

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

	json::JSON& PrimitivesNode = Root[SceneKeys::Primitives];
	TMap<uint32, UActorComponent*> UUIDToComp;
	TArray<uint32> RootUUIDs;

	// 1단계: 계층 구조 및 루트 컴포넌트 식별
	TMap<uint32, TArray<uint32>> ChildrenMap;
	TMap<uint32, uint32> NonSceneToRootMap;

	for (auto& [UUIDStr, Data] : PrimitivesNode.ObjectRange())
	{
		uint32 UUID = static_cast<uint32>(std::stoul(UUIDStr));
		if (Data.hasKey("ParentUUID"))
		{
			ChildrenMap[static_cast<uint32>(Data["ParentUUID"].ToInt())].push_back(UUID);
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

	auto InferActorClass = [](const string& CompType) -> string
	{
		if (CompType == "StaticMeshComp" || CompType == "UStaticMeshComponent") return "AStaticMeshActor";
		if (CompType.length() > 10 && CompType.substr(CompType.size() - 9) == "Component")
		{
			string BaseName = CompType.substr(0, CompType.size() - 9);
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
		FString CompType = PrimitivesNode[std::to_string(RootUUID)][SceneKeys::Type].ToString();
		
		AActor* NewActor = Cast<AActor>(FObjectFactory::Get().Create(InferActorClass(CompType)));
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

void FSceneSaveManager::DeserializePrimitivesToWorld(json::JSON& PrimitivesNode, UWorld* World)
{
    // ---------------------------------------------------------------
    // 1단계: 씬/비씬 노드 분류
    // OwnerRootUUID 키가 있으면 비씬 ActorComponent, 없으면 SceneComponent
    // ---------------------------------------------------------------
    std::unordered_map<uint32, json::JSON*> SceneNodeMap;
    std::unordered_map<uint32, json::JSON*> NonSceneNodeMap;
    std::unordered_map<uint32, std::vector<uint32>> ChildrenMap;
    std::vector<uint32> RootUUIDs;

    for (auto& Pair : PrimitivesNode.ObjectRange())
    {
        uint32 UUID = static_cast<uint32>(std::stoul(Pair.first));

        if (Pair.second.hasKey(SceneKeys::OwnerRootUUID))
        {
            NonSceneNodeMap[UUID] = &Pair.second;
        }
        else
        {
            SceneNodeMap[UUID] = &Pair.second;

            if (Pair.second.hasKey(SceneKeys::ParentUUID))
            {
                uint32 ParentID = static_cast<uint32>(Pair.second[SceneKeys::ParentUUID].ToInt());
                ChildrenMap[ParentID].push_back(UUID);
            }
            else
            {
                RootUUIDs.push_back(UUID);
            }
        }
    }

    // 타입 문자열로 Actor 클래스 이름 추론
    auto InferActorClass = [](const string& CompType) -> string
    {
        if (CompType.front() == 'U' && CompType.size() > 10 &&
            CompType.substr(CompType.size() - 9) == "Component")
        {
            return "A" + CompType.substr(1, CompType.size() - 10) + "Actor";
        }
        return "AActor";
    };

    // UUID → SceneComponent* 맵: SceneComponentRef 역직렬화에 사용
    std::unordered_map<uint32, USceneComponent*> UUIDToSceneComp;
    // 루트SceneComponent UUID → Actor* 맵: 비씬 컴포넌트 귀속에 사용
    std::unordered_map<uint32, AActor*> RootUUIDToActor;

    // ---------------------------------------------------------------
    // 2단계: SceneComponent 트리 복원 (기존 로직 + UUID 맵 구축)
    // ---------------------------------------------------------------
    std::function<void(uint32, USceneComponent*, AActor*)> CreateSceneComponent;
    CreateSceneComponent = [&](uint32 UUID, USceneComponent* ParentComp, AActor* Owner)
    {
        auto NodeIt = SceneNodeMap.find(UUID);
        if (NodeIt == SceneNodeMap.end()) return;

        json::JSON& PrimJSON = *NodeIt->second;
        if (!PrimJSON.hasKey(SceneKeys::Type)) return;

        string CompType = PrimJSON[SceneKeys::Type].ToString();
        if (CompType == "StaticMeshComp") CompType = "UStaticMeshComponent";

        USceneComponent* Comp = nullptr;
        if (!ParentComp)
        {
            // 루트 컴포넌트: 대응하는 Actor 생성
            // ActorClass가 저장되어 있으면 그대로, 없으면 컴포넌트 타입으로 추론 (하위 호환)
            //string ActorClass = PrimJSON.hasKey("ActorClass")
            //    ? PrimJSON["ActorClass"].ToString()
            //    : InferActorClass(CompType);
            //UObject* Obj = FObjectFactory::Get().Create(ActorClass);
            UObject* Obj = FObjectFactory::Get().Create(InferActorClass(CompType));
            AActor* NewActor = Cast<AActor>(Obj);
            if (!NewActor) return;

            NewActor->InitDefaultComponents();
            NewActor->SetWorld(World);
            if (ULevel* Level = World->GetPersistentLevel())
                Level->AddActor(NewActor);

            Comp  = NewActor->GetRootComponent();
            Owner = NewActor;

            RootUUIDToActor[UUID] = NewActor;
        }
        else
        {
            // 자식 컴포넌트: 직접 생성 후 부모에 연결
            UObject* Obj = FObjectFactory::Get().Create(CompType);
            if (!Obj || !Obj->IsA<USceneComponent>()) return;

            Comp = static_cast<USceneComponent*>(Obj);
            Owner->RegisterComponent(Comp);
            Comp->AttachToComponent(ParentComp);
        }

        // UUID → SceneComponent 매핑 등록
        UUIDToSceneComp[UUID] = Comp;

        // 프로퍼티 기반 역직렬화 (SceneComponentRef는 아직 연결 불필요)
        DeserializeProperties(Comp, PrimJSON, nullptr);
        Comp->MarkTransformDirty();

        // 자식 컴포넌트 재귀 생성
        auto ChildIt = ChildrenMap.find(UUID);
        if (ChildIt != ChildrenMap.end())
        {
            for (uint32 ChildUUID : ChildIt->second)
                CreateSceneComponent(ChildUUID, Comp, Owner);
        }
    };

    for (uint32 RootUUID : RootUUIDs)
        CreateSceneComponent(RootUUID, nullptr, nullptr);

    // ---------------------------------------------------------------
    // 3단계: 비씬 ActorComponent 복원 (UUID 맵이 완성된 후 실행)
    // SceneComponentRef 속성(예: UpdatedComponent)을 UUID로 해석한다.
    // ---------------------------------------------------------------
    for (auto& [UUID, NodePtr] : NonSceneNodeMap)
    {
        json::JSON& CompJSON = *NodePtr;
        if (!CompJSON.hasKey(SceneKeys::Type)) continue;

        uint32 OwnerRootUUID = static_cast<uint32>(CompJSON[SceneKeys::OwnerRootUUID].ToInt());
        auto ActorIt = RootUUIDToActor.find(OwnerRootUUID);
        if (ActorIt == RootUUIDToActor.end()) continue;

        AActor* OwnerActor = ActorIt->second;
        string CompType = CompJSON[SceneKeys::Type].ToString();

        UObject* Obj = FObjectFactory::Get().Create(CompType);
        if (!Obj || !Obj->IsA<UActorComponent>()) continue;
        if (Obj->IsA<USceneComponent>()) continue; // 안전장치

        UActorComponent* Comp = static_cast<UActorComponent*>(Obj);
        OwnerActor->RegisterComponent(Comp);

        // SceneComponentRef 포함 모든 프로퍼티 역직렬화
        DeserializeProperties(Comp, CompJSON, &UUIDToSceneComp);
    }

    if (World)
        World->SyncSpatialIndex();
}

/* @brief 현재 사용하지 않는 함수, 추후 Actor-Component 단위로 계층화를 시켜야 한다면 이쪽을 사용 */
USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
	string ClassName = Node[SceneKeys::ClassName].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ClassName);
	if (!Obj || !Obj->IsA<USceneComponent>()) return nullptr;

	USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
	Owner->RegisterComponent(Comp);

	// Restore properties
	if (Node.hasKey(SceneKeys::Properties)) {
		auto PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Comp, PropsJSON);
	}
	Comp->MarkTransformDirty();

	// Restore children recursively
	if (Node.hasKey(SceneKeys::Children)) {
		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange()) {
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner);
			if (Child) {
				Child->AttachToComponent(Comp);
			}
		}
	}

	return Comp;
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON,
                                              const std::unordered_map<uint32, USceneComponent*>* UUIDToSceneComp)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (auto& Prop : Descriptors) {
		FString JsonKey = Prop.Name;
		if (strcmp(Prop.Name, "StaticMesh") == 0)
			JsonKey = "ObjStaticMeshAsset";
		if (!PropsJSON.hasKey(JsonKey)) continue;

		auto Value = PropsJSON[JsonKey];
		DeserializePropertyValue(Prop, Value, UUIDToSceneComp);
		Comp->PostEditProperty(Prop.Name);
	}
}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value,
                                                 const std::unordered_map<uint32, USceneComponent*>* UUIDToSceneComp)
{
	switch (Prop.Type) {
	case EPropertyType::Bool:
		*static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
    case EPropertyType::Color: {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 3)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
		break;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::String:
		*static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
		break;

	case EPropertyType::Name:
		*static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
		break;

	case EPropertyType::SceneComponentRef: {
		// UUID를 USceneComponent* 포인터로 역직렬화 (UUID 맵이 있을 때만)
		if (!UUIDToSceneComp) break;
		int32 RefUUID = Value.ToInt();
		if (RefUUID <= 0) break;
		auto It = UUIDToSceneComp->find(static_cast<uint32>(RefUUID));
		if (It != UUIDToSceneComp->end())
			*static_cast<USceneComponent**>(Prop.ValuePtr) = It->second;
		break;
	}

	case EPropertyType::Vec3Array: {
		auto& Arr = *static_cast<TArray<FVector>*>(Prop.ValuePtr);
		Arr.clear();
		for (auto& elem : Value.ArrayRange()) {
			float X = static_cast<float>(elem[0].ToFloat());
			float Y = static_cast<float>(elem[1].ToFloat());
			float Z = static_cast<float>(elem[2].ToFloat());
			Arr.push_back(FVector(X, Y, Z));
		}
		break;
	}

	case EPropertyType::Enum: {
		*static_cast<int*>(Prop.ValuePtr) = Value.ToInt();
		break;
	}

	case EPropertyType::Material: {
		auto& Arr = *static_cast<TArray<UMaterialInterface*>*>(Prop.ValuePtr);
		Arr.clear();
		for (auto& elem : Value.ArrayRange())
		{
			UMaterialInterface* Mat = FResourceManager::Get().GetMaterialInterface(elem.ToString());
			Arr.push_back(Mat);
		}
		break;
	}

	default:
		break;
	}
}

void FSceneSaveManager::DeserializeCameraState(json::JSON& root, FEditorCameraState* OutCameraState /*= nullptr*/)
{
	using namespace json;
	// Perspective 카메라 상태 복원
	if (OutCameraState && root.hasKey(SceneKeys::PerspectiveCamera))
	{
		JSON Cam = root[SceneKeys::PerspectiveCamera];

		if (Cam.hasKey(SceneKeys::Location))
		{
			JSON Loc = Cam[SceneKeys::Location];
			OutCameraState->Location = FVector(
				static_cast<float>(Loc[0].ToFloat()),
				static_cast<float>(Loc[1].ToFloat()),
				static_cast<float>(Loc[2].ToFloat()));
		}
		if (Cam.hasKey(SceneKeys::Rotation))
		{
			JSON Rot = Cam[SceneKeys::Rotation];
			OutCameraState->Rotation = FRotator(
				static_cast<float>(Rot[0].ToFloat()),  // Pitch
				static_cast<float>(Rot[1].ToFloat()),  // Yaw
				static_cast<float>(Rot[2].ToFloat())); // Roll
		}
		
		// 수정: FOV, NearClip, FarClip이 배열([ ]) 형태로 들어오므로 0번째 인덱스로 접근
		if (Cam.hasKey(SceneKeys::FOV))
			OutCameraState->FOV = static_cast<float>(Cam[SceneKeys::FOV][0].ToFloat());
		if (Cam.hasKey(SceneKeys::NearClip))
			OutCameraState->NearClip = static_cast<float>(Cam[SceneKeys::NearClip][0].ToFloat());
		if (Cam.hasKey(SceneKeys::FarClip))
			OutCameraState->FarClip = static_cast<float>(Cam[SceneKeys::FarClip][0].ToFloat());

		OutCameraState->bValid = true;
	}
}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
	TArray<FString> Result;
	std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return Result;
	}

	for (auto& Entry : std::filesystem::directory_iterator(SceneDir))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
		{
			Result.push_back(FPaths::ToUtf8(Entry.path().stem().wstring()));
		}
	}
	return Result;
}
