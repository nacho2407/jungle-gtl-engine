#include "Core/CoreMinimal.h"
#include "SpriteAnimationActor.h"

#include "Engine/Component/Sprite/AnimatedSpriteComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ASpriteAnimationActor::ASpriteAnimationActor()
{
    auto* AnimatedSpriteComponent = new Engine::Component::UAnimatedSpriteComponent();
    AnimatedSpriteComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(AnimatedSpriteComponent, true);

    Name = "CubeActor";
}

Engine::Component::UAnimatedSpriteComponent* ASpriteAnimationActor::GetAnimatedSpriteComponent() const
{
    return Cast<Engine::Component::UAnimatedSpriteComponent>(RootComponent);
}

bool ASpriteAnimationActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ASpriteAnimationActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASpriteAnimationActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASpriteAnimationActor::GetMeshType() const
{
    if (const auto* CubeComponent = GetPrimitiveComponent())
    {
        return CubeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASpriteAnimationActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASpriteAnimationActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASpriteAnimationActor)