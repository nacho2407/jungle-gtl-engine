#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "Math/Vector.h"

class UWorld;

class FLuaWorldProxy
{
public:
    FLuaWorldProxy() = default;
    explicit FLuaWorldProxy(UWorld* InWorld) { SetWorld(InWorld); }

    void SetWorld(UWorld* InWorld);
    UWorld* ResolveWorld() const;

    bool IsValid() const;

    FLuaGameObjectProxy SpawnActor(const FString& ActorClassName);
    bool DestroyActor(const FLuaGameObjectProxy& ActorProxy);
    FVector GetActiveCameraForward() const;
    FVector GetActiveCameraRight() const;
    FVector GetActiveCameraUp() const;

private:
    uint32 WorldUUID = 0;
};
