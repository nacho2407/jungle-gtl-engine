#include "Core/CoreMinimal.h"
#include "AtlasSpriteActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

AAtlasSpriteActor::AAtlasSpriteActor()
{
    auto* SubUVComponent = new Engine::Component::USubUVComponent();
    SubUVComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(SubUVComponent, true);

    Name = "AtlasSpriteActor";
}

Engine::Component::USubUVComponent* AAtlasSpriteActor::GetSubUVTextureComponent() const
{
    return Cast<Engine::Component::USubUVComponent>(RootComponent);
}

bool AAtlasSpriteActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool AAtlasSpriteActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor AAtlasSpriteActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType AAtlasSpriteActor::GetMeshType() const
{
    if (const auto* PrimitiveComp = GetPrimitiveComponent())
    {
        return PrimitiveComp->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 AAtlasSpriteActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* AAtlasSpriteActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, AAtlasSpriteActor)
