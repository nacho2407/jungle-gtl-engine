#include "EffectActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"

AEffectActor::AEffectActor()
{
    auto* EffectComponent = new Engine::Component::USubUVComponent();
    EffectComponent->SetColor({0.8f, 0.8f, 0.8f, 1.f});
    AddOwnedComponent(EffectComponent, true);

    Name = "EffectActor";
}

Engine::Component::USubUVComponent* AEffectActor::GetEffectComponent() const
{
    return Cast<Engine::Component::USubUVComponent>(RootComponent);
}

bool AEffectActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool AEffectActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor AEffectActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType AEffectActor::GetMeshType() const
{
    if (const auto* EffectComponent = GetEffectComponent())
    {
        return EffectComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 AEffectActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* AEffectActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, AEffectActor)
