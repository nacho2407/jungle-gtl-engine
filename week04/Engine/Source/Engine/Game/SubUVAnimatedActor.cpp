#include "Core/CoreMinimal.h"
#include "SubUVAnimatedActor.h"
#include "Engine/Component/Sprite/SubUVAnimatedComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h" 

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ASubUVAnimatedActor::ASubUVAnimatedActor()
{
    auto* AnimatedComponent = new Engine::Component::USubUVAnimatedComponent();
    AnimatedComponent->SetColor({0.8f, 0.8f, 0.8f, 1.f});
    AddOwnedComponent(AnimatedComponent, true);

    Name = "SubUVAnimatedActor";
}

Engine::Component::USubUVAnimatedComponent* ASubUVAnimatedActor::GetSubUVAnimatedComponent() const
{
    return Cast<Engine::Component::USubUVAnimatedComponent>(RootComponent);
}

bool ASubUVAnimatedActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ASubUVAnimatedActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASubUVAnimatedActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASubUVAnimatedActor::GetMeshType() const
{
    // 변수명 CubeComponent -> PrimitiveComp 로 가독성 수정
    if (const auto* PrimitiveComp = GetPrimitiveComponent())
    {
        return PrimitiveComp->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASubUVAnimatedActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASubUVAnimatedActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASubUVAnimatedActor)