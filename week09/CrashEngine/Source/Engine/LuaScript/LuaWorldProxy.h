#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "Math/Vector.h"
#include "Object/FName.h"

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
    FLuaGameObjectProxy FindPlayer();
    FLuaGameObjectProxy FindActorByTag(const FString& Tag);
    TArray<FLuaGameObjectProxy> FindActorsByTag(const FString& Tag);
    bool SetCameraView(const FVector& Location,const FVector& Target, float FovDegrees);
    bool MoveActorWithBlock(const FLuaGameObjectProxy& ActorProxy, const FVector& Delta, const FString& BlockingTag);
    FVector GetActiveCameraForward() const;
    FVector GetActiveCameraRight() const;
    FVector GetActiveCameraUp() const;

private:
    uint32 WorldUUID = 0;
};
