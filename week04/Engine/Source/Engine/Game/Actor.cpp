#include "Actor.h"
#include "Engine/Component/SceneComponent.h"

AActor::AActor() = default;

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
    RootComponent = InRootComponent;
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
            if (bMakeRootComponent)
            {
                RootComponent = InComponent;
            }

            return;
        }
    }

    OwnedComponents.push_back(InComponent);
    if (bMakeRootComponent || RootComponent == nullptr)
    {
        RootComponent = InComponent;
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
