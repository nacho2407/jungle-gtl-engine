#include "Core/CoreMinimal.h"
#include "ConeActor.h"

#include "Engine/Component/Mesh/ConeComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

AConeActor::AConeActor()
{
    auto* ConeComponent = new Engine::Component::UConeComponent();
    ConeComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(ConeComponent, true);

    Name = "ConeActor";
}

Engine::Component::UConeComponent* AConeActor::GetConeComponent() const
{
    return Cast<Engine::Component::UConeComponent>(RootComponent);
}

bool AConeActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool AConeActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor AConeActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType AConeActor::GetMeshType() const
{
    if (const auto* ConeComponent = GetConeComponent())
    {
        return ConeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 AConeActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* AConeActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, AConeActor)
