#include "TriangleActor.h"

#include "Engine/Component/Mesh/TriangleComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ATriangleActor::ATriangleActor()
{
    auto* TriangleComponent = new Engine::Component::UTriangleComponent();
    TriangleComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(TriangleComponent, true);

    Name = "TriangleActor";
}

Engine::Component::UTriangleComponent* ATriangleActor::GetTriangleComponent() const
{
    return Cast<Engine::Component::UTriangleComponent>(RootComponent);
}

bool ATriangleActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ATriangleActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ATriangleActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ATriangleActor::GetMeshType() const
{
    if (const auto* TriangleComponent = GetTriangleComponent())
    {
        return TriangleComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ATriangleActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ATriangleActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ATriangleActor)
