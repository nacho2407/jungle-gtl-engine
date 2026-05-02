#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Serialization/PrefabManager.h"

namespace
{
    UWorld* GetEngineAPIWorld()
    {
        return GEngine ? GEngine->GetWorld() : nullptr;
    }

    sol::table ActorsToLuaTable(sol::this_state State, const TArray<AActor*>& Actors)
    {
        sol::state_view Lua(State);
        sol::table Result = Lua.create_table();

        int32 Index = 1;
        for (AActor* Actor : Actors)
        {
            if (Actor)
            {
                Result[Index++] = Actor;
            }
        }

        return Result;
    }

    bool LuaTagsTableContainsActorTags(const sol::table& TagsTable, AActor* Actor)
    {
        if (!Actor)
        {
            return false;
        }

        bool bHasAnyTag = false;
        for (const auto& Pair : TagsTable)
        {
            sol::object Value = Pair.second;
            if (Value.valid() && Value.is<FString>())
            {
                bHasAnyTag = true;
                if (!Actor->HasTag(Value.as<FString>()))
                {
                    return false;
                }
            }
        }
        return bHasAnyTag;
    }

    bool ActorMatchesTypeName(const AActor* Actor, const FString& TypeName)
    {
        if (!Actor || TypeName.empty())
        {
            return false;
        }

        for (const FTypeInfo* Type = Actor->GetTypeInfo(); Type != nullptr; Type = Type->Parent)
        {
            if (Type->name && TypeName == Type->name)
            {
                return true;
            }
        }
        return false;
    }
}

namespace FLuaEngineAPI
{
    void BindWorld(sol::state& Lua, sol::table& API)
    {
        sol::table World = Lua.create_table();

        World["FindActorByName"] = [](const FString& Name) -> AActor*
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (!ActiveWorld)
            {
                return nullptr;
            }

            for (AActor* Actor : ActiveWorld->GetActors())
            {
                if (Actor && Actor->GetName() == Name)
                {
                    return Actor;
                }
            }
            return nullptr;
        };

        World["FindActorsByName"] = [](sol::this_state State, const FString& Name)
        {
            TArray<AActor*> Matches;

            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld)
            {
                for (AActor* Actor : ActiveWorld->GetActors())
                {
                    if (Actor && Actor->GetName() == Name)
                    {
                        Matches.push_back(Actor);
                    }
                }
            }

            return ActorsToLuaTable(State, Matches);
        };

        World["FindActorsByTag"] = [](sol::this_state State, const FString& Tag)
        {
            TArray<AActor*> Matches;

            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld)
            {
                for (AActor* Actor : ActiveWorld->GetActors())
                {
                    if (Actor && Actor->HasTag(Tag))
                    {
                        Matches.push_back(Actor);
                    }
                }
            }

            return ActorsToLuaTable(State, Matches);
        };

        World["FindActorsByTags"] = [](sol::this_state State, sol::table Tags)
        {
            TArray<AActor*> Matches;

            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld)
            {
                for (AActor* Actor : ActiveWorld->GetActors())
                {
                    if (LuaTagsTableContainsActorTags(Tags, Actor))
                    {
                        Matches.push_back(Actor);
                    }
                }
            }

            return ActorsToLuaTable(State, Matches);
        };

        World["GetAllActors"] = [](sol::this_state State)
        {
            TArray<AActor*> Matches;
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld)
            {
                Matches = ActiveWorld->GetActors();
            }
            return ActorsToLuaTable(State, Matches);
        };

        World["FindActorByTag"] = [](const FString& Tag) -> AActor*
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (!ActiveWorld)
            {
                return nullptr;
            }

            for (AActor* Actor : ActiveWorld->GetActors())
            {
                if (Actor && Actor->HasTag(Tag))
                {
                    return Actor;
                }
            }
            return nullptr;
        };

        World["FindActorsByType"] = [](sol::this_state State, const FString& TypeName)
        {
            TArray<AActor*> Matches;

            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld)
            {
                for (AActor* Actor : ActiveWorld->GetActors())
                {
                    if (ActorMatchesTypeName(Actor, TypeName))
                    {
                        Matches.push_back(Actor);
                    }
                }
            }

            return ActorsToLuaTable(State, Matches);
        };

        World["GetActorCount"] = []() -> int32
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            return ActiveWorld ? static_cast<int32>(ActiveWorld->GetActors().size()) : 0;
        };

        World["IsValidActor"] = [](AActor* Actor) -> bool
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (!ActiveWorld || !Actor)
            {
                return false;
            }

            for (AActor* WorldActor : ActiveWorld->GetActors())
            {
                if (WorldActor == Actor)
                {
                    return true;
                }
            }
            return false;
        };

        World["SpawnActor"] = [](const FString& TypeName) -> AActor*
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (!ActiveWorld)
            {
                return nullptr;
            }

            AActor* Actor = ActiveWorld->SpawnActorByTypeName(TypeName);
            if (Actor)
            {
                ActiveWorld->SyncSpatialIndex();
            }
            return Actor;
        };

        World["SpawnActorFromPrefab"] = [](const FString& RelativePath) -> AActor*
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (!ActiveWorld)
            {
                return nullptr;
            }

            return FPrefabManager::SpawnActorFromPrefab(ActiveWorld, RelativePath);
        };

        World["DestroyActor"] = [](AActor* Actor)
        {
            UWorld* ActiveWorld = GetEngineAPIWorld();
            if (ActiveWorld && Actor)
            {
                ActiveWorld->DestroyActor(Actor);
            }
        };

        API["World"] = World;
    }
}
