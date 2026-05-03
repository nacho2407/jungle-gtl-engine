#include "LuaGameObjectProxy.h"

#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

FLuaGameObjectProxy::FLuaGameObjectProxy(AActor* InActor)
{
    SetActor(InActor);
}

void FLuaGameObjectProxy::SetActor(AActor* InActor)
{
    ActorUUID = InActor ? InActor->GetUUID() : 0;

    UWorld* World = InActor ? InActor->GetWorld() : nullptr;
    WorldUUID = World ? World->GetUUID() : 0;
}

AActor* FLuaGameObjectProxy::ResolveActor() const
{
    if (ActorUUID == 0)
    {
        return nullptr;
    }

    AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
    if (!Actor)
    {
        return nullptr;
    }

    if (WorldUUID != 0)
    {
        UWorld* World = Actor->GetWorld();
        if (!World || World->GetUUID() != WorldUUID)
        {
            return nullptr;
        }
    }

    return Actor;
}

FVector FLuaGameObjectProxy::GetLocation() const
{
    AActor* Actor = ResolveActor();
    return Actor ? Actor->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaGameObjectProxy::SetLocation(const FVector& InLocation)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->SetActorLocation(InLocation);
}

void FLuaGameObjectProxy::AddWorldOffset(const FVector& Delta)
{
    AActor* Actor = ResolveActor();
    if (!Actor)
    {
        return;
    }

    Actor->AddActorWorldOffset(Delta);
}

void FLuaGameObjectProxy::ApplyDamage(float Damage, const FLuaGameObjectProxy& Instigator)
{
    if (!Actor)
    {
        return;
    }

    Actor->TakeDamage(Damage, Instigator.GetActor());
}

void FLuaGameObjectProxy::PrintLocation() const
{
    const FVector Location = GetLocation();
    UE_LOG(Lua, Info,
           "Actor %u Location = (%.3f, %.3f, %.3f)",
           GetUUID(),
           Location.X,
           Location.Y,
           Location.Z);
}
