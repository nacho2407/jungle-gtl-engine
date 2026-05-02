#include "LuaWorldProxy.h"

#include "Core/Logging/LogMacros.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"

namespace
{
FString NormalizeLuaActorClassName(const FString& Name)
{
    if (Name == "Actor")
        return "AActor";
    if (Name == "StaticMeshActor")
        return "AStaticMeshActor";
    if (Name == "PointLightActor")
        return "APointLightActor";
    if (Name == "SpotLightActor")
        return "ASpotLightActor";
    if (Name == "DirectionalLightActor")
        return "ADirectionalLightActor";
    if (Name == "HeightFogActor")
        return "AHeightFogActor";
    if (Name == "DecalActor")
        return "ADecalActor";

    return Name;
}

bool IsLuaSpawnableActorClassName(const FString& ClassName)
{
    return ClassName == "AActor" 
		|| ClassName == "AStaticMeshActor" 
		|| ClassName == "APointLightActor" 
		|| ClassName == "ASpotLightActor" 
		|| ClassName == "ADirectionalLightActor" 
		|| ClassName == "AHeightFogActor" 
		|| ClassName == "ADecalActor";
}
} // namespace

void FLuaWorldProxy::SetWorld(UWorld* InWorld)
{
    WorldUUID = InWorld ? InWorld->GetUUID() : 0;
}

UWorld* FLuaWorldProxy::ResolveWorld() const
{
    if (WorldUUID == 0)
    {
        return nullptr;
    }

    return Cast<UWorld>(UObjectManager::Get().FindByUUID(WorldUUID));
}

bool FLuaWorldProxy::IsValid() const
{
    return ResolveWorld() != nullptr;
}

FLuaGameObjectProxy FLuaWorldProxy::SpawnActor(const FString& ActorClassName)
{
    UWorld* World = ResolveWorld();
    if (!World)
    {
        return FLuaGameObjectProxy();
    }

    const FString NormalizedName = NormalizeLuaActorClassName(ActorClassName);
    if (!IsLuaSpawnableActorClassName(NormalizedName))
    {
        UE_LOG(Lua, Warning,
               "Lua tried to spawn disallowed actor class: %s",
               ActorClassName.c_str());
        return FLuaGameObjectProxy();
    }

    AActor* Actor = World->SpawnActorByClassName(NormalizedName);
    if (!Actor)
    {
        UE_LOG(Lua, Warning,
               "Lua failed to spawn actor class: %s",
               NormalizedName.c_str());
        return FLuaGameObjectProxy();
    }

    return FLuaGameObjectProxy(Actor);
}

bool FLuaWorldProxy::DestroyActor(const FLuaGameObjectProxy& ActorProxy)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    if (!World || !Actor)
    {
        return false;
    }

    World->DestroyActor(Actor);
    return true;
}