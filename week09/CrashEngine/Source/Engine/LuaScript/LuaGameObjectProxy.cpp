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