#include "LuaWorldProxy.h"

#include "Component/CameraComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/LuaScriptActor.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Physics/CollisionManager.h"

namespace
{
constexpr float kLuaBlockedMoveStepLength = 0.05f;

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
    if (Name == "LuaScriptActor")
        return "ALuaScriptActor";
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
		|| ClassName == "ALuaScriptActor" 
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

    if (ALuaScriptActor* LuaScriptActor = Cast<ALuaScriptActor>(Actor))
    {
        LuaScriptActor->InitDefaultComponents();
    }

    return FLuaGameObjectProxy(Actor);
}

bool FLuaWorldProxy::DestroyActor(const FLuaGameObjectProxy& ActorProxy)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    if (!World || !Actor || Actor->GetWorld() != World)
    {
        return false;
    }

    World->DestroyActor(Actor);
    return true;
}

FLuaGameObjectProxy FLuaWorldProxy::FindPlayer()
{
    UWorld* World = ResolveWorld();
    if (!World)
    {
        return FLuaGameObjectProxy();
    }

    TArray<AActor*> Actors = World->GetActors();
    for (AActor* Actor : Actors)
    {
        TArray<FName> Tags = Actor->GetTags();
        for (FName Tag : Tags)
        {
            if (Tag==FName("Player"))
            {
                return FLuaGameObjectProxy(Actor);
            }
        }
    }
    return FLuaGameObjectProxy();
}

FLuaGameObjectProxy FLuaWorldProxy::FindActorByTag(const FString& Tag)
{
    UWorld* World = ResolveWorld();
    if (!World || Tag.empty())
    {
        return FLuaGameObjectProxy();
    }

    TArray<AActor*> Actors = World->GetActors();
    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->HasTag(Tag))
        {
            return FLuaGameObjectProxy(Actor);
        }
    }

    return FLuaGameObjectProxy();
}

TArray<FLuaGameObjectProxy> FLuaWorldProxy::FindActorsByTag(const FString& Tag)
{
    TArray<FLuaGameObjectProxy> Result;

    UWorld* World = ResolveWorld();
    if (!World || Tag.empty())
    {
        return Result;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->HasTag(Tag))
        {
            Result.emplace_back(Actor);
        }
    }

    return Result;
}

bool FLuaWorldProxy::SetCameraView(const FVector& Location,const FVector& Target, float FovDegrees)
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    if (!Camera) return false;
    Camera->SetWorldLocation(Location);
    Camera->LookAt(Target);
    Camera->SetFOV(FovDegrees * DEG_TO_RAD);
    return true;
}

bool FLuaWorldProxy::MoveActorWithBlock(const FLuaGameObjectProxy& ActorProxy, const FVector& Delta, const FString& BlockingTag)
{
    UWorld* World = ResolveWorld();
    AActor* Actor = ActorProxy.GetActor();
    FCollisionManager* CollisionManager = World ? World->GetCollisionManager() : nullptr;
    if (!World || !CollisionManager || !Actor || Actor->GetWorld() != World)
    {
        return false;
    }

    return CollisionManager->MoveActorWithBlock(Actor, Delta, BlockingTag, kLuaBlockedMoveStepLength);
}



FVector FLuaWorldProxy::GetActiveCameraForward() const
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    return Camera ? Camera->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
}

FVector FLuaWorldProxy::GetActiveCameraRight() const
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    return Camera ? Camera->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
}

FVector FLuaWorldProxy::GetActiveCameraUp() const
{
    UWorld* World = ResolveWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    return Camera ? Camera->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
}
