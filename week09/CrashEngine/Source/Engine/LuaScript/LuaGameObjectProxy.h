#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class AActor;

/**
 * @brief Lua 스크립트에 AActor를 그대로 공개하면 엔진 크래시가 자주 발생할 수 있으므로, AActor에 대한 프록시 클래스를 만들어 Lua에서 안전하게 사용
 */
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