#include "CubeActor.h"

#include "Engine/Component/Mesh/CubeComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ACubeActor::ACubeActor()
{
    auto* CubeComponent = new Engine::Component::UCubeComponent();
    RootComponent = CubeComponent;
    OwnedComponents.push_back(CubeComponent);

    Name = "CubeActor";
}

Engine::Component::UCubeComponent* ACubeActor::GetCubeComponent() const
{
    return Cast<Engine::Component::UCubeComponent>(RootComponent);
}

bool ACubeActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ACubeActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ACubeActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return ToColor(PrimitiveComponent->GetColor());
    }

    return FColor::White();
}

EBasicMeshType ACubeActor::GetMeshType() const
{
    if (const auto* CubeComponent = GetCubeComponent())
    {
        return CubeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ACubeActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ACubeActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ACubeActor)
