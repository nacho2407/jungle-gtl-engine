// 직렬화 영역의 세부 동작을 구현합니다.
#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <unordered_map>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/LightComponentBase.h"
#include "Component/StaticMeshComponent.h"
#include "Component/CameraComponent.h"
#include "GameFramework/StaticMeshActor.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Profiling/PlatformTime.h"

// ---- JSON vector helpers ---------------------------------------------------

static void WriteVec3(json::JSON& Obj, const char* Key, const FVector& V)
{
    json::JSON arr = json::Array();
    arr.append(static_cast<double>(V.X));
    arr.append(static_cast<double>(V.Y));
    arr.append(static_cast<double>(V.Z));
    Obj[Key] = arr;
}

static FVector ReadVec3(json::JSON& Arr)
{
    FVector out(0, 0, 0);
    int i = 0;
    for (auto& e : Arr.ArrayRange())
    {
        if (i == 0)
            out.X = static_cast<float>(e.ToFloat());
        else if (i == 1)
            out.Y = static_cast<float>(e.ToFloat());
        else if (i == 2)
            out.Z = static_cast<float>(e.ToFloat());
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------------------

namespace SceneKeys
{
static constexpr const char* Version = "Version";
static constexpr const char* Name = "Name";
static constexpr const char* ClassName = "ClassName";
static constexpr const char* WorldType = "WorldType";
static constexpr const char* ContextName = "ContextName";
static constexpr const char* ContextHandle = "ContextHandle";
static constexpr const char* Actors = "Actors";
static constexpr const char* Visible = "bVisible";
static constexpr const char* RootComponent = "RootComponent";
static constexpr const char* NonSceneComponents = "NonSceneComponents";
static constexpr const char* Properties = "Properties";
static constexpr const char* Children = "Children";
static constexpr const char* ShadowResolution = "Shadow Resolution";
} // namespace SceneKeys

static const char* WorldTypeToString(EWorldType Type)
{
    switch (Type)
    {
    case EWorldType::Game:
        return "Game";
    case EWorldType::PIE:
        return "PIE";
    default:
        return "Editor";
    }
}

static EWorldType StringToWorldType(const string& Str)
{
    if (Str == "Game")
        return EWorldType::Game;
    if (Str == "PIE")
        return EWorldType::PIE;
    return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext, UCameraComponent* PerspectiveCam)
{
    using namespace json;

    if (!WorldContext.World)
        return;

    string FinalName = InSceneName.empty()
                           ? "Save_" + GetCurrentTimeStamp()
                           : InSceneName;

    std::wstring SceneDir = GetSceneDirectory();
    std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
    std::filesystem::create_directories(SceneDir);

    JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectiveCam);
    Root[SceneKeys::Version] = 2;
    Root[SceneKeys::Name] = FinalName;

    std::ofstream File(FileDestination);
    if (File.is_open())
    {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx, UCameraComponent* PerspectiveCam)
{
    using namespace json;
    JSON w = json::Object();
    w[SceneKeys::ClassName] = World->GetClass()->GetName();
    w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
    w[SceneKeys::ContextName] = Ctx.ContextName;
    w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

    // ---- Primitives: gather static mesh components into a top-level block
    JSON Primitives = json::Object();
    std::unordered_map<AActor*, string> ActorPrimitiveKey;

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
            continue;

        for (UActorComponent* Comp : Actor->GetComponents())
        {
            if (!Comp)
                continue;
            if (!Comp->IsA<UStaticMeshComponent>())
                continue;

            UStaticMeshComponent* S = static_cast<UStaticMeshComponent*>(Comp);

            JSON p = json::Object();

            const FMatrix& M = S->GetWorldMatrix();
            FVector loc = M.GetLocation();
            FVector rot = M.GetEuler();
            FVector scale = M.GetScale();

            p["ObjStaticMeshAsset"] = S->GetStaticMeshPath();

            JSON locArr = json::Array();
            locArr.append(static_cast<double>(loc.X));
            locArr.append(static_cast<double>(loc.Y));
            locArr.append(static_cast<double>(loc.Z));
            p["Location"] = locArr;

            JSON rotArr = json::Array();
            rotArr.append(static_cast<double>(rot.X));
            rotArr.append(static_cast<double>(rot.Y));
            rotArr.append(static_cast<double>(rot.Z));
            p["Rotation"] = rotArr;

            JSON scaleArr = json::Array();
            scaleArr.append(static_cast<double>(scale.X));
            scaleArr.append(static_cast<double>(scale.Y));
            scaleArr.append(static_cast<double>(scale.Z));
            p["Scale"] = scaleArr;

            p["Type"] = "StaticMeshComp";

            // Note: per design, material/UV overrides are stored on the Actor->RootComponent Properties
            // and are NOT duplicated inside the Primitives block to avoid redundancy.

            // Use the Actor's UUID as the primitive key to avoid positional/index coupling
            string key = std::to_string(Actor->GetUUID());
            Primitives[key] = p;
            ActorPrimitiveKey[Actor] = key;

            // only first static mesh component per actor is exported as primitive
            break;
        }
    }

    if (Primitives.size() > 0)
    {
        w["Primitives"] = Primitives;
    }

    // ---- Actors: serialize and attach PrimitiveKey when present ----
    JSON Actors = json::Array();
    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
            continue;
        JSON a = SerializeActor(Actor);
        auto it = ActorPrimitiveKey.find(Actor);
        if (it != ActorPrimitiveKey.end())
        {
            a["PrimitiveKey"] = it->second;
        }
        Actors.append(a);
    }
    w[SceneKeys::Actors] = Actors;

    // ---- Perspective camera ----
    JSON cam = SerializeCamera(PerspectiveCam);
    if (cam.size() > 0)
    {
        w["PerspectiveCamera"] = cam;
    }

    return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
    using namespace json;
    JSON a = json::Object();
    a[SceneKeys::ClassName] = Actor->GetClass()->GetName();
    a[SceneKeys::Visible] = Actor->IsVisible();

    // RootComponent 트리 직렬화
    if (Actor->GetRootComponent())
    {
        a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent());
    }

    // Non-scene components
    JSON NonScene = json::Array();
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp)
            continue;
        if (Comp->IsA<USceneComponent>())
            continue;

        JSON c = json::Object();
        c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
        c[SceneKeys::Properties] = SerializeProperties(Comp);
        NonScene.append(c);
    }
    a[SceneKeys::NonSceneComponents] = NonScene;

    return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
    using namespace json;
    JSON c = json::Object();
    c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
    c[SceneKeys::Properties] = SerializeProperties(Comp);

    JSON Children = json::Array();
    for (USceneComponent* Child : Comp->GetChildren())
    {
        if (!Child)
            continue;
        Children.append(SerializeSceneComponentTree(Child));
    }
    c[SceneKeys::Children] = Children;

    return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
    using namespace json;
    JSON props = json::Object();

    TArray<FPropertyDescriptor> Descriptors;
    Comp->GetEditableProperties(Descriptors);

    for (const auto& Prop : Descriptors)
    {
        // if (Prop.Name == "Static Mesh") continue; // Primitives 블록에 이미 저장됨
        props[Prop.Name] = SerializePropertyValue(Prop);
    }

    if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
    {
        props[SceneKeys::ShadowResolution] = JSON(static_cast<int32>(LightComponent->GetShadowResolution()));
    }

    return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
    using namespace json;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
        return JSON(*static_cast<bool*>(Prop.ValuePtr));

    case EPropertyType::Int:
        return JSON(*static_cast<int32*>(Prop.ValuePtr));

    case EPropertyType::Float:
        return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

    case EPropertyType::Vec3:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        return arr;
    }
    case EPropertyType::Rotator:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        return arr;
    }
    case EPropertyType::Vec4:
    case EPropertyType::Color4:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        arr.append(static_cast<double>(v[3]));
        return arr;
    }
    case EPropertyType::String:
    case EPropertyType::StaticMeshRef:
    case EPropertyType::ComponentRef:
        return JSON(*static_cast<FString*>(Prop.ValuePtr));

    case EPropertyType::MaterialSlot:
    {
        const FMaterialSlot* Slot = static_cast<const FMaterialSlot*>(Prop.ValuePtr);
        JSON obj = json::Object();
        obj["Path"] = JSON(Slot->Path);
        return obj;
    }

    case EPropertyType::ByteBool:
        return JSON(static_cast<bool>(*static_cast<uint8_t*>(Prop.ValuePtr) != 0));

    case EPropertyType::Name:
        return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

    default:
        return JSON();
    }
}

// ---- Camera helpers ----

json::JSON FSceneSaveManager::SerializeCamera(UCameraComponent* Cam)
{
    using namespace json;
    JSON cam = json::Object();
    if (!Cam)
        return cam;

    const FMatrix& M = Cam->GetWorldMatrix();
    WriteVec3(cam, "Location", M.GetLocation());
    WriteVec3(cam, "Rotation", M.GetEuler());

    const FCameraState& S = Cam->GetCameraState();
    cam["FOV"] = static_cast<double>(S.FOV);
    cam["NearClip"] = static_cast<double>(S.NearZ);
    cam["FarClip"] = static_cast<double>(S.FarZ);

    return cam;
}

void FSceneSaveManager::DeserializePrimitives(json::JSON& Primitives, UWorld* World, std::unordered_map<string, AActor*>& OutCreatedActors)
{
    using namespace json;

    // Octree 일괄 삽입을 위해 생성된 Actor를 모아둠
    TArray<AActor*> CreatedActors;
    CreatedActors.reserve(Primitives.size());

    for (auto& kv : Primitives.ObjectRange())
    {
        const string& Key = kv.first;
        JSON& Entry = kv.second;

        if (!Entry.hasKey("Type"))
            continue;
        if (Entry["Type"].ToString() != "StaticMeshComp")
            continue;

        string MeshPath = Entry.hasKey("ObjStaticMeshAsset") ? Entry["ObjStaticMeshAsset"].ToString() : string("None");

        // Spawn a static mesh actor and initialize
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
        if (!Actor)
            continue;
        Actor->InitDefaultComponents(FString(MeshPath));
        OutCreatedActors[Key] = Actor;

        // Location / Rotation / Scale — 인덱스 직접 접근으로 iterator 순회 제거
        FVector Loc(0, 0, 0), Rot(0, 0, 0), Scale(1, 1, 1);

        if (Entry.hasKey("Location"))
        {
            JSON& arr = Entry["Location"];
            Loc.X = static_cast<float>(arr[0].ToFloat());
            Loc.Y = static_cast<float>(arr[1].ToFloat());
            Loc.Z = static_cast<float>(arr[2].ToFloat());
        }
        if (Entry.hasKey("Rotation"))
        {
            JSON& arr = Entry["Rotation"];
            Rot.X = static_cast<float>(arr[0].ToFloat());
            Rot.Y = static_cast<float>(arr[1].ToFloat());
            Rot.Z = static_cast<float>(arr[2].ToFloat());
        }
        if (Entry.hasKey("Scale"))
        {
            JSON& arr = Entry["Scale"];
            Scale.X = static_cast<float>(arr[0].ToFloat());
            Scale.Y = static_cast<float>(arr[1].ToFloat());
            Scale.Z = static_cast<float>(arr[2].ToFloat());
        }

        Actor->SetActorLocation(Loc);
        Actor->SetActorRotation(Rot);
        Actor->SetActorScale(Scale);
        World->RemoveActorToOctree(Actor);
        World->InsertActorToOctree(Actor);

        CreatedActors.push_back(Actor);
    }

    // Octree 일괄 삽입
    /*for (AActor* Actor : CreatedActors)
    {
        World->InsertActorToOctree(Actor);
    }*/
}

void FSceneSaveManager::DeserializeCamera(json::JSON& CameraJSON, FPerspectiveCameraData& OutCam)
{
    using namespace json;
    if (CameraJSON.JSONType() == JSON::Class::Null)
        return;

    if (CameraJSON.hasKey("Location"))
        OutCam.Location = ReadVec3(CameraJSON["Location"]);
    if (CameraJSON.hasKey("Rotation"))
        OutCam.Rotation = ReadVec3(CameraJSON["Rotation"]);
    if (CameraJSON.hasKey("FOV"))
    {
        auto& Val = CameraJSON["FOV"];
        float fov = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
        // 엔진 내부는 라디안 — π(~3.14)를 넘으면 degree로 간주하고 변환
        if (fov > 3.14159265f)
            fov *= (3.14159265f / 180.0f);
        OutCam.FOV = fov;
    }
    if (CameraJSON.hasKey("NearClip"))
    {
        auto& Val = CameraJSON["NearClip"];
        OutCam.NearClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
    }
    if (CameraJSON.hasKey("FarClip"))
    {
        auto& Val = CameraJSON["FarClip"];
        OutCam.FarClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
    }
    OutCam.bValid = true;
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam)
{
    using json::JSON;
    std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
    if (!File.is_open())
    {
        std::cerr << "Failed to open file at target destination" << std::endl;
        return;
    }

    string FileContent((std::istreambuf_iterator<char>(File)),
                       std::istreambuf_iterator<char>());

    JSON root = JSON::Load(FileContent);

    string ClassName = root[SceneKeys::ClassName].ToString();
    ClassName = ClassName.empty() ? "UWorld" : ClassName; // Default to "World" if ClassName is missing
    UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
    if (!WorldObj || !WorldObj->IsA<UWorld>())
        return;

    UWorld* World = static_cast<UWorld*>(WorldObj);

    EWorldType WorldType = root.hasKey(SceneKeys::WorldType)
                               ? StringToWorldType(root[SceneKeys::WorldType].ToString())
                               : EWorldType::Editor;
    FString ContextName = root.hasKey(SceneKeys::ContextName)
                              ? root[SceneKeys::ContextName].ToString()
                              : "Loaded Scene";
    FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
                                ? root[SceneKeys::ContextHandle].ToString()
                                : ContextName;

    World->InitWorld();

    // Deserialize Primitives (top-level) and Camera first
    std::unordered_map<string, AActor*> CreatedFromPrimitives;
    if (root.hasKey("Primitives"))
    {
        JSON& Prims = root["Primitives"];
        DeserializePrimitives(Prims, World, CreatedFromPrimitives);
    }

    // "PerspectiveCamera" 우선, 구버전 "Camera" 키도 지원
    const char* CamKey = root.hasKey("PerspectiveCamera") ? "PerspectiveCamera"
                         : root.hasKey("Camera")          ? "Camera"
                                                          : nullptr;
    if (CamKey)
    {
        JSON& Cam = root[CamKey];
        DeserializeCamera(Cam, OutCam);
    }

    // Deserialize Actors
    if (root.hasKey(SceneKeys::Actors))
    {
        for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange())
        {
            string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
            // If this actor references a PrimitiveKey and that primitive already created an actor,
            // prefer the primitive-created actor and update it instead of creating a duplicate.
            AActor* Actor = nullptr;
            if (ActorJSON.hasKey("PrimitiveKey"))
            {
                string pk = ActorJSON["PrimitiveKey"].ToString();
                auto it = CreatedFromPrimitives.find(pk);
                if (it != CreatedFromPrimitives.end())
                {
                    Actor = it->second;
                }
            }

            if (!Actor)
            {
                UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
                if (!ActorObj || !ActorObj->IsA<AActor>())
                    continue;
                Actor = static_cast<AActor*>(ActorObj);
                World->AddActor(Actor);
            }

            if (ActorJSON.hasKey(SceneKeys::Visible))
            {
                Actor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
            }

            // RootComponent 트리 복원
            if (ActorJSON.hasKey(SceneKeys::RootComponent))
            {
                JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
                if (Actor->GetRootComponent())
                {
                    // Merge properties into existing root component created by primitives
                    DeserializeSceneComponentIntoExisting(Actor->GetRootComponent(), RootJSON, Actor);
                }
                else
                {
                    USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor);
                    if (Root)
                        Actor->SetRootComponent(Root);
                }
            }

            // Non-scene components 복원
            if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
            {
                for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange())
                {
                    string CompClass = CompJSON[SceneKeys::ClassName].ToString();
                    UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
                    if (!CompObj || !CompObj->IsA<UActorComponent>())
                        continue;

                    UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
                    Actor->RegisterComponent(Comp);

                    if (CompJSON.hasKey(SceneKeys::Properties))
                    {
                        JSON& PropsJSON = CompJSON[SceneKeys::Properties];
                        DeserializeProperties(Comp, PropsJSON);
                    }
                }
            }

            World->RemoveActorToOctree(Actor);
            World->InsertActorToOctree(Actor);
        }
    }

    OutWorldContext.WorldType = WorldType;
    OutWorldContext.World = World;
    OutWorldContext.ContextName = ContextName;
    OutWorldContext.ContextHandle = FName(ContextHandle);
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
    string ClassName = Node[SceneKeys::ClassName].ToString();
    UObject* Obj = FObjectFactory::Get().Create(ClassName, Owner);
    if (!Obj || !Obj->IsA<USceneComponent>())
        return nullptr;

    USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
    Owner->RegisterComponent(Comp);

    // Restore properties
    if (Node.hasKey(SceneKeys::Properties))
    {
        json::JSON& PropsJSON = Node[SceneKeys::Properties];
        DeserializeProperties(Comp, PropsJSON);
    }
    Comp->MarkTransformDirty();

    // Restore children recursively
    if (Node.hasKey(SceneKeys::Children))
    {
        for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange())
        {
            USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner);
            if (Child)
            {
                Child->AttachToComponent(Comp);
            }
        }
    }

    return Comp;
}

void FSceneSaveManager::DeserializeSceneComponentIntoExisting(USceneComponent* Existing, json::JSON& Node, AActor* Owner)
{
    using namespace json;
    if (!Existing)
        return;

    if (Node.hasKey(SceneKeys::Properties))
    {
        JSON& PropsJSON = Node[SceneKeys::Properties];
        DeserializeProperties(Existing, PropsJSON);
    }

    // Children: merge into existing children by order; create new children if missing
    if (Node.hasKey(SceneKeys::Children))
    {
        auto& ChildrenJSON = Node[SceneKeys::Children];
        auto ExistingChildren = Existing->GetChildren();

        size_t idx = 0;
        for (auto& ChildJSON : ChildrenJSON.ArrayRange())
        {
            if (idx < ExistingChildren.size())
            {
                DeserializeSceneComponentIntoExisting(ExistingChildren[idx], const_cast<json::JSON&>(ChildJSON), Owner);
            }
            else
            {
                USceneComponent* NewChild = DeserializeSceneComponentTree(const_cast<json::JSON&>(ChildJSON), Owner);
                if (NewChild)
                    NewChild->AttachToComponent(Existing);
            }
            idx++;
        }
    }
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
    TArray<FPropertyDescriptor> Descriptors;
    Comp->GetEditableProperties(Descriptors);

    for (auto& Prop : Descriptors)
    {
        if (!PropsJSON.hasKey(Prop.Name.c_str()))
            continue;
        json::JSON& Value = PropsJSON[Prop.Name.c_str()];
        DeserializePropertyValue(Prop, Value);
        Comp->PostEditProperty(Prop.Name.c_str());
    }

    if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
    {
        if (PropsJSON.hasKey(SceneKeys::ShadowResolution))
        {
            LightComponent->SetShadowResolution(static_cast<uint32>(PropsJSON[SceneKeys::ShadowResolution].ToInt()));
            Comp->PostEditProperty(SceneKeys::ShadowResolution);
        }
    }

    // 2nd pass: PostEditProperty가 새 프로퍼티를 추가할 수 있음
    // (예: SetStaticMesh → MaterialSlots 생성 → "Element N" 디스크립터 추가)
    TArray<FPropertyDescriptor> Descriptors2;
    Comp->GetEditableProperties(Descriptors2);

    for (size_t i = Descriptors.size(); i < Descriptors2.size(); ++i)
    {
        auto& Prop = Descriptors2[i];
        if (!PropsJSON.hasKey(Prop.Name.c_str()))
            continue;
        json::JSON& Value = PropsJSON[Prop.Name.c_str()];
        DeserializePropertyValue(Prop, Value);
        Comp->PostEditProperty(Prop.Name.c_str());
    }
}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
    switch (Prop.Type)
    {
    case EPropertyType::Bool:
        *static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
        break;

    case EPropertyType::ByteBool:
        *static_cast<uint8_t*>(Prop.ValuePtr) = Value.ToBool() ? 1 : 0;
        break;

    case EPropertyType::Int:
        *static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
        break;

    case EPropertyType::Float:
        *static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
        break;

    case EPropertyType::Vec3:
    {
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
    case EPropertyType::Rotator:
    {
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
    case EPropertyType::Vec4:
    case EPropertyType::Color4:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 4)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
        break;
    }
    case EPropertyType::String:
    case EPropertyType::StaticMeshRef:
    case EPropertyType::ComponentRef:
        *static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
        break;

    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
        if (Value.hasKey("Path"))
            Slot->Path = Value["Path"].ToString();
        break;
    }

    case EPropertyType::Name:
        *static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
        break;

    default:
        break;
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
