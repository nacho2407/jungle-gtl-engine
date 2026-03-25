#include "SpriteActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Sprite/SpriteComponent.h"

ASpriteActor::ASpriteActor()
{
    auto* SpriteComponent = new Engine::Component::USpriteComponent();
    SpriteComponent->SetColor({0.8f, 0.8f, 0.8f, 1.f});
    AddOwnedComponent(SpriteComponent, true);

    Name = "SpriteActor";
}

Engine::Component::USpriteComponent* ASpriteActor::GetSpriteComponent() const
{
    return Cast<Engine::Component::USpriteComponent>(RootComponent);
}

bool ASpriteActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ASpriteActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASpriteActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASpriteActor::GetMeshType() const
{
    if (const auto* SpriteComponent = GetSpriteComponent())
    {
        return SpriteComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASpriteActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASpriteActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASpriteActor)
