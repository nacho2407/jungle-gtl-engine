#include "Actor.h"
#include "Engine/Component/Core/SceneComponent.h"

#include <algorithm>

#include "Engine/Component/Text/AtlasTextComponent.h"

AActor::AActor()
{
    UUIDTextComponent = new Engine::Component::UAtlasTextComponent();
    static_cast<Engine::Component::UAtlasTextComponent*>(UUIDTextComponent)->SetFontPath("Font\\Comic_Sans_MS\\Comic_Sans_MS.json");
    static_cast<Engine::Component::UAtlasTextComponent*>(UUIDTextComponent)->SetText("Test");
    AddOwnedComponent(UUIDTextComponent);
}

AActor::~AActor()
{
    // Actor가 직접 생성해 들고 있던 SceneComponent들을 정리합니다.
    for (Engine::Component::USceneComponent* Component : OwnedComponents)
    {
        delete Component;
    }

    OwnedComponents.clear();
    RootComponent = nullptr;
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

    const auto RootIterator = std::find(OwnedComponents.begin(), OwnedComponents.end(), RootComponent);
    if (RootIterator != OwnedComponents.end() && RootIterator != OwnedComponents.begin())
    {
        Engine::Component::USceneComponent* Root = *RootIterator;
        OwnedComponents.erase(RootIterator);
        OwnedComponents.insert(OwnedComponents.begin(), Root);
    }
}

void AActor::AddOwnedComponent(Engine::Component::USceneComponent* InComponent,
                               bool bMakeRootComponent)
{
    if (InComponent == nullptr)
    {
        return;
    }

    for (Engine::Component::USceneComponent* ExistingComponent : OwnedComponents)
    {
        if (ExistingComponent == InComponent)
        {
            InComponent->SetOwnerActor(this);

            if (bMakeRootComponent)
            {
                SetRootComponent(InComponent);
            }
            else if (RootComponent != nullptr && InComponent != RootComponent &&
                     InComponent->GetAttachParent() == nullptr)
            {
                InComponent->AttachToComponent(RootComponent);
            }

            return;
        }
    }

    OwnedComponents.push_back(InComponent);
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

FMatrix AActor::GetWorldMatrix() const
{
    //  현재 설계 상 RootComponent는 PrimitiveComponent로 간주
    if (RootComponent != nullptr)
    {
        return RootComponent->GetRelativeMatrix();
    }

    return FMatrix::Identity;
}

EBasicMeshType AActor::GetMeshType() const
{
    // 기본 Actor는 렌더 대상이 아니므로 의미 없는 기본값
    // 실제 렌더 Actor가 override 하도록 둠
    return EBasicMeshType::Cube;
}
