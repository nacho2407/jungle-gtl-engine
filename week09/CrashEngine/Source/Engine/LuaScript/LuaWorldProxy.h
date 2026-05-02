#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaGameObjectProxy.h"

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

private:
    uint32 WorldUUID = 0;
};
