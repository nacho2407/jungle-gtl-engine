#include "Core/CoreMinimal.h"
#include "CylinderActor.h"

#include "Engine/Component/Mesh/CylinderComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ACylinderActor::ACylinderActor()
{
    auto* CylinderComponent = new Engine::Component::UCylinderComponent();
    CylinderComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(CylinderComponent, true);

    Name = "CylinderActor";
}

Engine::Component::UCylinderComponent* ACylinderActor::GetCylinderComponent() const
{
    return Cast<Engine::Component::UCylinderComponent>(RootComponent);
}

bool ACylinderActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ACylinderActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ACylinderActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ACylinderActor::GetMeshType() const
{
    if (const auto* CylinderComponent = GetCylinderComponent())
    {
        return CylinderComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ACylinderActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ACylinderActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ACylinderActor)
