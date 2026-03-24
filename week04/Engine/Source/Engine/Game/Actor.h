#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class USceneComponent;
    class UAtlasComponent;
} // namespace Engine::Component

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
                           bool                                bMakeRootComponent = false);

    // Render bridge용 최소 API
    virtual bool IsRenderable() const { return false; }
    virtual bool IsVisible() const { return true; }
    virtual bool IsSelected() const { return false; }
    virtual bool IsHovered() const { return false; }
    
    //  Bound 보여주기
    virtual bool IsShowBounds() const { return RootComponent->IsShowBounds(); }
    virtual void SetShowBounds(bool bInShowBounds) { RootComponent->SetShowBounds(bInShowBounds); }

    virtual FMatrix        GetWorldMatrix() const;
    virtual FColor         GetColor() const { return FColor::White(); }
    virtual EBasicMeshType GetMeshType() const;
    virtual uint32         GetObjectId() const { return 0; }

    Engine::Component::UAtlasComponent* GetUUIDTextComponent() const { return UUIDTextComponent; }

  protected:
    Engine::Component::USceneComponent*         RootComponent = nullptr;
    TArray<Engine::Component::USceneComponent*> OwnedComponents;
    Engine::Component::UAtlasComponent*          UUIDTextComponent = nullptr;

    bool bPickable = true;
};
