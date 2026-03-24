#include "Core/CoreMinimal.h"
#include "SphereActor.h"

#include "Engine/Component/Mesh/SphereComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return {InColor.X, InColor.Y, InColor.Z, InColor.W};
    }
} // namespace

ASphereActor::ASphereActor()
{
    auto* SphereComponent = new Engine::Component::USphereComponent();
    SphereComponent->SetColor({ 0.8f, 0.8f, 0.8f, 1.f });
    AddOwnedComponent(SphereComponent, true);

    Name = "SphereActor";
}

Engine::Component::USphereComponent* ASphereActor::GetSphereComponent() const
{
    return Cast<Engine::Component::USphereComponent>(RootComponent);
}

bool ASphereActor::IsRenderable() const
{
    return GetPrimitiveComponent() != nullptr;
}

bool ASphereActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ASphereActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ASphereActor::GetMeshType() const
{
    if (const auto* CubeComponent = GetSphereComponent())
    {
        return CubeComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ASphereActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ASphereActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ASphereActor)