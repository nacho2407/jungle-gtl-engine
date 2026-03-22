#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"
#include "Renderer/Types/BasicMeshType.h"

class USceneComponent;

class ENGINE_API AActor : public UObject
{
    DECLARE_RTTI(AActor, UObject)

  public:
    AActor();
    virtual ~AActor() = default;

    bool IsPickable() const;
    void SetPickable(bool bInPickable);

    USceneComponent* GetRootComponent() const { return RootComponent; }

    // Render bridge용 최소 API
    virtual bool IsRenderable() const { return false; }
    virtual bool IsVisible() const { return true; }
    virtual bool IsSelected() const { return false; }
    virtual bool IsHovered() const { return false; }

    virtual FMatrix        GetWorldMatrix() const;
    virtual FColor         GetColor() const { return FColor::White(); }
    virtual EBasicMeshType GetMeshType() const;
    virtual uint32         GetObjectId() const { return 0; }

  protected:
    USceneComponent*         RootComponent = nullptr;
    TArray<USceneComponent*> OwnedComponents;

    bool bPickable = true;
};