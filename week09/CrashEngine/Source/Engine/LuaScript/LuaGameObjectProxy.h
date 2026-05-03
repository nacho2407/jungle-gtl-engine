#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaComponentProxy.h"
#include "Math/Vector.h"

class AActor;

class FLuaGameObjectProxy
{
public:
    FLuaGameObjectProxy() = default;
    explicit FLuaGameObjectProxy(AActor* InActor);

    void SetActor(AActor* InActor);
    AActor* ResolveActor() const;
    AActor* GetActor() const { return ResolveActor(); }

    bool IsValid() const { return ResolveActor() != nullptr; }

    uint32 GetUUID() const { return ActorUUID; }
    uint32 GetWorldUUID() const { return WorldUUID; }

    FVector GetLocation() const;
    void SetLocation(const FVector& InLocation);

    void AddWorldOffset(const FVector& Delta);

    void ApplyDamage(float Damage, const FLuaGameObjectProxy& Instigator);
    bool HasTag(const FString& Tag) const;
    void AddTag(const FString& Tag);
    void RemoveTag(const FString& Tag);

    void PrintLocation() const;

	FLuaComponentProxy AddComponent(const FString& ComponentType);
    FLuaComponentProxy GetComponent(const FString& ComponentType, int32 Index = 0) const;

public:
    FVector Velocity = FVector(0.0f, 0.0f, 0.0f);

private:
    uint32 ActorUUID = 0;

	// World 내 Actor 검증용
    uint32 WorldUUID = 0;
};
