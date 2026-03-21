#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, TArray<UWorld*>& Scene) {
    using namespace json;

    string FinalName = InSceneName.empty()
        ? "Save_" + GetCurrentTimeStamp()
        : InSceneName;

    std::wstring SceneDir = GetSceneDirectory();
    std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);

    std::filesystem::create_directories(SceneDir);

    JSON Root;

    // Metadata
    Root["Scene"]["Version"] = 1;
    Root["Scene"]["Name"] = FinalName;

    JSON Objects = json::Array();

    for (UWorld* World : Scene) {
        if (!World) continue;

        // Serialize world itself
        Objects.append(SerializeObject(World));

        for (AActor* Actor : World->GetActors()) {
            if (!Actor) continue;

            // Serialize actor
            Objects.append(SerializeObject(Actor));

            for (USceneComponent* Component : Actor->GetComponents()) {
                if (!Component) continue;

                // Serialize component
                Objects.append(SerializeObject(Component));
            }
        }
    }

    Root["Scene"]["Objects"] = Objects;

    std::ofstream File(FileDestination);
    if (File.is_open()) {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeObject(UObject* Object) {
    using namespace json;
    JSON j = json::Object();

    if (!Object) return j;

    // Base UObject fields
    j["ClassName"] = Object->GetTypeInfo()->name;
    j["UUID"] = Object->GetUUID();
    j["InternalIndex"] = Object->GetInternalIndex();

    if (Object->IsA<USceneComponent>()) {
        USceneComponent* Comp = Object->Cast<USceneComponent>();
        FVector Loc = Comp->GetRelativeLocation();
        j["Location"] = SerializeVector(Loc.X, Loc.Y, Loc.Z);
        FVector Rot = Comp->GetRelativeRotation();
        j["Rotation"] = SerializeVector(Rot.X, Rot.Y, Rot.Z);
        FVector Scale = Comp->GetRelativeScale();
        j["Scale"] = SerializeVector(Scale.X, Scale.Y, Scale.Z);

        // Parent in the component hierarchy
        j["ParentUUID"] = Comp->GetParent()
            ? (int)Comp->GetParent()->GetUUID()
            : 0;
    }

    if (Object->IsA<AActor>()) {
        AActor* Actor = Object->Cast<AActor>();
        j["bVisible"] = Actor->IsVisible();
        j["RootComponentUUID"] = Actor->GetRootComponent()
            ? (int)Actor->GetRootComponent()->GetUUID()
            : 0;

        // Guard against null world
        j["OwningWorldUUID"] = Actor->GetWorld()
            ? (int)Actor->GetWorld()->GetUUID()
            : 0;
    }

    return j;
}

json::JSON FSceneSaveManager::SerializeVector(float X, float Y, float Z) {
    json::JSON v = json::Object();
    v["X"] = X;
    v["Y"] = Y;
    v["Z"] = Z;
    return v;
}

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, TArray<UWorld*>& Scene) {
    // Purge current scene
    for (UWorld* World : Scene) {
        World->EndPlay();
        UObjectManager::Get().DestroyObject(World);
    }
    Scene.clear();
    using json::JSON;
    std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
    if (!File.is_open()) {
        // Failed to open file at target destination
        std::cerr << "Failed to open file at target destination" << std::endl;
        return;
    }

    string FileContent((std::istreambuf_iterator<char>(File)),
        std::istreambuf_iterator<char>());

    JSON root = JSON::Load(FileContent);
    TMap<uint32, UObject*> uuidObjectMap;
    uint32 MaxUUID = 0;
    TArray<uint32> Worlds;

    // Retrieve UObject info
    for (auto& JSONObject : root["Scene"]["Objects"].ArrayRange()) {
        string ClassName = JSONObject["ClassName"].ToString();
        UObject* Obj = FObjectFactory::Get().Create(ClassName);
        if (!Obj || !Obj->IsA<UObject>()) {
            // Either ClassName is not a valid UObject type, or factory has not been linked yet
            continue;
        }

        // Give essential information
        Obj->SetInternalIndex(JSONObject["InternalIndex"].ToInt());
        uint32 LoadedUUID = JSONObject["UUID"].ToInt(); MaxUUID = MaxUUID > LoadedUUID ? MaxUUID : LoadedUUID;
        Obj->SetUUID(LoadedUUID);

        // Register Object to UUID Map
        uuidObjectMap[LoadedUUID] = Obj;

        // Perform necessary transformations for USceneComponents
        if (Obj->IsA<USceneComponent>()) {
            // GIve Space vectors
            DeserializeSpaceVectors(Obj->Cast<USceneComponent>(), JSONObject);

            // Handle camera objects
            if (Obj->IsA<UCameraComponent>()) { DecodeCamera(Obj->Cast<UCameraComponent>(), JSONObject); }
        }

        if (Obj->IsA<UWorld>()) {
            Worlds.push_back(Obj->GetUUID());
        }
    }

    // Resolve parent-owning relationship
    LinkReferences(uuidObjectMap, root["Scene"]["Objects"]);

    // Reload Scene with cooked worlds
    for (auto i : Worlds) {
        auto it = uuidObjectMap.find(i);
        if (it != uuidObjectMap.end()) {
            Scene.push_back(static_cast<UWorld*>(it->second));
        }
    }

    // Reset UUID counting logic
    EngineStatics::ResetUUIDGeneration(MaxUUID + 1);
}

void FSceneSaveManager::LinkReferences(const TMap<uint32, UObject*>& uuidMap, json::JSON Savedata) {
    using namespace json;
    // Pass 1: restore all parent-child relationships
    for (auto& JSONObject : Savedata.ArrayRange()) {
        uint32 cachedUUID = JSONObject["UUID"].ToInt();
        auto it = uuidMap.find(cachedUUID);
        if (it == uuidMap.end()) continue;
        UObject* Obj = it->second;

        if (Obj->IsA<USceneComponent>()) {
            USceneComponent* SceneComp = Obj->Cast<USceneComponent>();
            uint32 ParentUUID = JSONObject["ParentUUID"].ToInt();
            if (ParentUUID) {
                auto ParentIt = uuidMap.find(ParentUUID);
                if (ParentIt != uuidMap.end()) {
                    SceneComp->SetParent(
                        static_cast<USceneComponent*>(ParentIt->second));
                }
            }
        }
    }

    // Pass 2: set root components and world relationships
    for (auto& JSONObject : Savedata.ArrayRange()) {
        uint32 cachedUUID = JSONObject["UUID"].ToInt();
        auto it = uuidMap.find(cachedUUID);
        if (it == uuidMap.end()) continue;
        UObject* Obj = it->second;

        if (Obj->IsA<AActor>()) {
            AActor* Actor = Obj->Cast<AActor>();

            uint32 WorldUUID = JSONObject["OwningWorldUUID"].ToInt();
            if (WorldUUID) {
                auto WorldIt = uuidMap.find(WorldUUID);
                if (WorldIt != uuidMap.end()) {
                    UWorld* World = static_cast<UWorld*>(WorldIt->second);
                    Actor->SetWorld(World);
                    World->AddActor(Actor);
                }
            }

            uint32 RootCompUUID = JSONObject["RootComponentUUID"].ToInt();
            if (RootCompUUID) {
                auto RootIt = uuidMap.find(RootCompUUID);
                if (RootIt != uuidMap.end()) {
                    auto* RootComp = static_cast<USceneComponent*>(RootIt->second);
                    Actor->SetRootComponent(RootComp);
                    RootComp->SetOwner(Actor);
                }
            }

            Actor->SetVisible(JSONObject["bVisible"].ToBool());
        }
    }
}

void FSceneSaveManager::DeserializeSpaceVectors(USceneComponent* SceneComp, json::JSON& Savedata) {
    auto& Pos = Savedata["Location"];
    auto& Rot = Savedata["Rotation"];
    auto& Scale = Savedata["Scale"];
    SceneComp->SetRelativeLocation(FVector(static_cast<float>(Pos["X"].ToFloat()), static_cast<float>(Pos["Y"].ToFloat()), static_cast<float>(Pos["Z"].ToFloat())));
    SceneComp->SetRelativeRotation(FVector(static_cast<float>(Rot["X"].ToFloat()), static_cast<float>(Rot["Y"].ToFloat()), static_cast<float>(Rot["Z"].ToFloat())));
    SceneComp->SetRelativeScale(FVector(static_cast<float>(Scale["X"].ToFloat()), static_cast<float>(Scale["Y"].ToFloat()), static_cast<float>(Scale["Z"].ToFloat())));
}

void FSceneSaveManager::DecodeCamera(UCameraComponent* Camera, json::JSON& Savedata) {
    // TODO
}

void FSceneSaveManager::DecodePrimitiveComponents(UPrimitiveComponent* Prim, json::JSON& Savedata) {
    // Placeholder. No features to add yet
}

void FSceneSaveManager::OverwriteSave(const string& filepath, UWorld* World) {
    // Check if file exists at target destination

    // If yes, delete

    // Create json save
}

string FSceneSaveManager::GetCurrentTimeStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);

    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList() {
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