#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class USceneComponent;
}

class ENGINE_API AActor : public UObject
{
    DECLARE_RTTI(AActor, UObject)

  public:
    AActor();
    ~AActor() override;

    bool IsPickable() const;
    void SetPickable(bool bInPickable);

    Engine::Component::USceneComponent* GetRootComponent() const { return RootComponent; }
    const TArray<Engine::Component::USceneComponent*>& GetOwnedComponents() const
    {
        return OwnedComponents;
    }
    void SetRootComponent(Engine::Component::USceneComponent* InRootComponent);
    void AddOwnedComponent(Engine::Component::USceneComponent* InComponent,
                           bool bMakeRootComponent = false);

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
    Engine::Component::USceneComponent*         RootComponent = nullptr;
    TArray<Engine::Component::USceneComponent*> OwnedComponents;

    bool bPickable = true;
};
