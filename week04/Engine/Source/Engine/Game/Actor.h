#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"

class USceneComponent;

class ENGINE_API AActor : public UObject
{
    DECLARE_RTTI(AActor, UObject)
  public:
    AActor();
    virtual ~AActor() = default;

    protected:
    USceneComponent* RootComponent;
      TArray<USceneComponent*> OwnedComponents;
};
