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
    Root[SceneKeys::Version] = 3; 
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
                CollectComponentsFlat(RootComp, 0, Primitives);
        }
    }
    return Primitives;
}

// 재귀 함수: Comp 및 모든 자손 컴포넌트를 OutPrimitives 에 평탄하게 수집
// ParentID == 0 은 부모 없음(루트 컴포넌트)을 의미 (UUID는 1부터 시작)
void FSceneSaveManager::CollectComponentsFlat(USceneComponent* Comp, uint32 ParentID, json::JSON& OutPrimitives)
{
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

void FSceneSaveManager::DeserializePrimitivesToWorld(json::JSON& PrimitivesNode, UWorld* World)
{
    // 1단계: UUID → JSON 노드 맵, ParentUUID → 자식 UUID 목록 맵 구성
    std::unordered_map<uint32, json::JSON*> NodeMap;
    std::unordered_map<uint32, std::vector<uint32>> ChildrenMap;
    std::vector<uint32> RootUUIDs;

    for (auto& Pair : PrimitivesNode.ObjectRange())
    {
        uint32 UUID = static_cast<uint32>(std::stoul(Pair.first));
        NodeMap[UUID] = &Pair.second;

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

    // 2단계: 루트부터 자손을 재귀적으로 생성·연결
    std::function<void(uint32, USceneComponent*, AActor*)> CreateComponent;
    CreateComponent = [&](uint32 UUID, USceneComponent* ParentComp, AActor* Owner)
    {
        auto NodeIt = NodeMap.find(UUID);
        if (NodeIt == NodeMap.end()) return;

        json::JSON& PrimJSON = *NodeIt->second;
        if (!PrimJSON.hasKey(SceneKeys::Type)) return;

        string CompType = PrimJSON[SceneKeys::Type].ToString();
        if (CompType == "StaticMeshComp") CompType = "UStaticMeshComponent";

        USceneComponent* Comp = nullptr;
        if (!ParentComp)
        {
            // 루트 컴포넌트: 대응하는 Actor 생성
            UObject* Obj = FObjectFactory::Get().Create(InferActorClass(CompType));
            AActor* NewActor = Cast<AActor>(Obj);
            if (!NewActor) return;

            NewActor->InitDefaultComponents();
            NewActor->SetWorld(World);
            if (ULevel* Level = World->GetPersistentLevel())
                Level->AddActor(NewActor);

            Comp  = NewActor->GetRootComponent();
            Owner = NewActor;
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

        // 프로퍼티 기반 역직렬화
        DeserializeProperties(Comp, PrimJSON);
        Comp->MarkTransformDirty();

        // 자식 컴포넌트 재귀 생성
        auto ChildIt = ChildrenMap.find(UUID);
        if (ChildIt != ChildrenMap.end())
        {
            for (uint32 ChildUUID : ChildIt->second)
                CreateComponent(ChildUUID, Comp, Owner);
        }
    };

    for (uint32 RootUUID : RootUUIDs)
        CreateComponent(RootUUID, nullptr, nullptr);

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

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (auto& Prop : Descriptors) {
		FString JsonKey = Prop.Name;
		if (strcmp(Prop.Name, "StaticMesh") == 0) 
			JsonKey = "ObjStaticMeshAsset";
		if (!PropsJSON.hasKey(JsonKey)) continue;
		
		auto Value = PropsJSON[JsonKey];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name);
	}
}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
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
