#include "Actor.h"
#include "Engine/Component/Core/SceneComponent.h"

#include <algorithm>
#include <stdlib.h>
#include <string>

#include "Engine/Component/Text/UUIDComponent.h"

AActor::AActor() = default;

AActor::~AActor()
{
    for (Engine::Component::USceneComponent* Component : OwnedComponents)
    {
        delete Component;
    }

    OwnedComponents.clear();
    RootComponent = nullptr;
}

void AActor::Tick(float DeltaTime)
{
    for (auto& Component : OwnedComponents)
    {
        if (Component)
        {
            Component->Update(DeltaTime);
        }
    }
}

bool AActor::IsPickable() const { return bPickable; }

void AActor::SetPickable(bool bInPickable) { bPickable = bInPickable; }

void AActor::SetRootComponent(Engine::Component::USceneComponent* InRootComponent)
{
    if (InRootComponent == nullptr)
    {
        RootComponent = nullptr;
        return;
    }

    const auto ExistingComponentIterator =
        std::find(OwnedComponents.begin(), OwnedComponents.end(), InRootComponent);
    if (ExistingComponentIterator == OwnedComponents.end())
    {
        OwnedComponents.push_back(InRootComponent);
    }

    InRootComponent->SetOwnerActor(this);
    InRootComponent->DetachFromParent();
    RootComponent = InRootComponent;

    const auto RootIterator =
        std::find(OwnedComponents.begin(), OwnedComponents.end(), RootComponent);
    if (RootIterator != OwnedComponents.end() && RootIterator != OwnedComponents.begin())
    {
        Engine::Component::USceneComponent* Root = *RootIterator;
        OwnedComponents.erase(RootIterator);
        OwnedComponents.insert(OwnedComponents.begin(), Root);
    }

    EnsureUUIDDebugComponent();
    RefreshUUIDDebugComponent();
}

void AActor::AddOwnedComponent(Engine::Component::USceneComponent* InComponent,
                               bool                                bMakeRootComponent)
{
    if (InComponent == nullptr)
    {
        return;
    }

    const auto ExistingComponentIterator =
        std::find(OwnedComponents.begin(), OwnedComponents.end(), InComponent);

    if (ExistingComponentIterator == OwnedComponents.end())
    {
        OwnedComponents.push_back(InComponent);
    }

    InComponent->SetOwnerActor(this);

    if (bMakeRootComponent || RootComponent == nullptr)
    {
        SetRootComponent(InComponent);
    }
    else if (InComponent != RootComponent && InComponent->GetAttachParent() == nullptr)
    {
        InComponent->AttachToComponent(RootComponent);
    }
}

void AActor::RemoveOwnedComponent(Engine::Component::USceneComponent* InComponent)
{
    if (InComponent == nullptr || InComponent == RootComponent)
    {
        return;
    }

    auto It = std::find(OwnedComponents.begin(), OwnedComponents.end(), InComponent);
    if (It != OwnedComponents.end())
    {
        OwnedComponents.erase(It);

        // Ensure proper detachment from hierarchy
        InComponent->DetachFromParent();

        // Prevent memory leak by manually deleting the object
        delete InComponent;
    }
}

void AActor::EnsureUUIDDebugComponent()
{
    if (RootComponent == nullptr)
    {
        return;
    }

    if (UUIDTextComponent == nullptr)
    {
        UUIDTextComponent = new Engine::Component::UUUIDComponent();
        AddOwnedComponent(UUIDTextComponent, false);
    }
    else if (UUIDTextComponent != RootComponent &&
             UUIDTextComponent->GetAttachParent() != RootComponent)
    {
        UUIDTextComponent->AttachToComponent(RootComponent);
    }
}

void AActor::RefreshUUIDDebugComponent()
{
    if (UUIDTextComponent == nullptr)
    {
        return;
    }

    UUIDTextComponent->RefreshFromOwner();
}

FMatrix AActor::GetWorldMatrix() const
{
    if (RootComponent != nullptr)
    {
        return RootComponent->GetWorldMatrix();
    }

    return FMatrix::Identity;
}

EBasicMeshType AActor::GetMeshType() const { return EBasicMeshType::Cube; }

AActor* AActor::Clone() const
{
    // TODO: Implement deep copy of actor and its components
    return nullptr;
}

REGISTER_CLASS(, AActor)
