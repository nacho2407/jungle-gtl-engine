#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class AActor;

class FLuaGameObjectProxy
{
public:
    FLuaGameObjectProxy() = default;
    explicit FLuaGameObjectProxy(AActor* InActor)
        : Actor(InActor)
    {
    }

    void SetActor(AActor* InActor) { Actor = InActor; }
    AActor* GetActor() const { return Actor; }

    bool IsValid() const { return Actor != nullptr; }

    uint32 GetUUID() const;

    FVector GetLocation() const;
    void SetLocation(const FVector& InLocation);

    void AddWorldOffset(const FVector& Delta);

    void PrintLocation() const;

public:
    FVector Velocity = FVector(0.0f, 0.0f, 0.0f);

private:
    AActor* Actor = nullptr;
};