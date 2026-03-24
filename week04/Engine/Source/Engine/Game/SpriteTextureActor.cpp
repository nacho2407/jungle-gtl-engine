#include "Core/CoreMinimal.h"
#include "SpriteTextureActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Sprite/SpriteComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ASpriteTextureActor::ASpriteTextureActor()
{
    auto* SpriteComponent = new Engine::Component::USpriteComponent();
    SpriteComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(SpriteComponent, true);

    Name = "ConeActor";
}

Engine::Component::USpriteComponent* ASpriteTextureActor::GetSpriteTextureComponent() const
{
    return Cast<Engine::Component::USpriteComponent>(RootComponent);
}

bool ASpriteTextureActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ASpriteTextureActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASpriteTextureActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASpriteTextureActor::GetMeshType() const
{
    if (const auto* ConeComponent = GetPrimitiveComponent())
    {
        return ConeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASpriteTextureActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASpriteTextureActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASpriteTextureActor)
