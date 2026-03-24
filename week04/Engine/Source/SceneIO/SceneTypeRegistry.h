#pragma once

#include "Core/CoreMinimal.h"

class AActor;

namespace Engine::Component
{
    class USceneComponent;
}

class ENGINE_API FSceneTypeRegistry
{
  public:
    static FString ResolveActorTypeName(const AActor& Actor);
    static FString ResolveComponentTypeName(const Engine::Component::USceneComponent& Component);

    static AActor* ConstructActor(const FString& TypeName, bool* OutKnownType = nullptr);
    static Engine::Component::USceneComponent* ConstructComponent(const FString& TypeName,
                                                                 bool* OutKnownType = nullptr);
};
