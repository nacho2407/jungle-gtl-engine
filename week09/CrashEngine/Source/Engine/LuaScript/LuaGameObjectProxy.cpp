#include "LuaGameObjectProxy.h"

#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"

uint32 FLuaGameObjectProxy::GetUUID() const
{
    return Actor ? Actor->GetUUID() : 0;
}

FVector FLuaGameObjectProxy::GetLocation() const
{
    return Actor ? Actor->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaGameObjectProxy::SetLocation(const FVector& InLocation)
{
    if (!Actor)
    {
        return;
    }

    Actor->SetActorLocation(InLocation);
}

void FLuaGameObjectProxy::AddWorldOffset(const FVector& Delta)
{
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

bool FLuaGameObjectProxy::HasTag(const FString& Tag) const
{
    return Actor ? Actor->HasTag(Tag) : false;
}

void FLuaGameObjectProxy::AddTag(const FString& Tag)
{
    if (Actor)
    {
        Actor->AddTag(Tag);
    }
}

void FLuaGameObjectProxy::RemoveTag(const FString& Tag)
{
    if (Actor)
    {
        Actor->RemoveTag(Tag);
    }
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
