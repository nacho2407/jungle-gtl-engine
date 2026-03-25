#pragma once

#include "Core/CoreMinimal.h"
#include "CoreUObject/Object.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class USceneComponent;
    class UUUIDComponent;
} // namespace Engine::Component

class ENGINE_API AActor : public UObject
{
    DECLARE_RTTI(AActor, UObject)

  public:
    AActor();
    ~AActor() override;

    void Tick(float DeltaTime);

    bool IsPickable() const;
    void SetPickable(bool bInPickable);

    Engine::Component::USceneComponent* GetRootComponent() const { return RootComponent; }

    FVector GetLocation() const
    {
        if (RootComponent)
        {
            return RootComponent->GetRelativeLocation();
        }
        return FVector();
    }
    void SetLocation(FVector InLocation) const
    {
        if (RootComponent)
        {
            return RootComponent->SetRelativeLocation(InLocation);
        }
    }

    FVector GetScale() const
    {
        if (RootComponent)
        {
            return RootComponent->GetRelativeScale3D();
        }
        return FVector::OneVector;
    }
    void SetScale(FVector InScale) const
    {
        if (RootComponent)
        {
            return RootComponent->SetRelativeScale3D(InScale);
        }
    }

    FQuat GetRotation() const
    {
        if (RootComponent)
        {
            return RootComponent->GetRelativeQuaternion();
        }
        return FQuat::Identity;
    }
    void SetRotion(FQuat InRotation) const
    {
        if (RootComponent)
        {
            return RootComponent->SetRelativeRotation(InRotation);
        }
    }

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

    Engine::Component::UUUIDComponent* GetUUIDTextComponent() const { return UUIDTextComponent; }
    void                               EnsureUUIDDebugComponent();
    void                               RefreshUUIDDebugComponent();

  protected:
    Engine::Component::USceneComponent*         RootComponent = nullptr;
    TArray<Engine::Component::USceneComponent*> OwnedComponents;
    Engine::Component::UUUIDComponent*          UUIDTextComponent = nullptr;

    bool bPickable = true;
};
