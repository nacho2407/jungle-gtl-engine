#include "Core/CoreMinimal.h"
#include "SubUVTextureActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ASubUVTextureActor::ASubUVTextureActor()
{
    auto* SubUVComponent = new Engine::Component::USubUVComponent();
    SubUVComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(SubUVComponent, true);

    Name = "ConeActor";
}

Engine::Component::USubUVComponent* ASubUVTextureActor::GetSubUVTextureComponent() const
{
    return Cast<Engine::Component::USubUVComponent>(RootComponent);
}

bool ASubUVTextureActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ASubUVTextureActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASubUVTextureActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASubUVTextureActor::GetMeshType() const
{
    if (const auto* ConeComponent = GetPrimitiveComponent())
    {
        return ConeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASubUVTextureActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASubUVTextureActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASubUVTextureActor)
