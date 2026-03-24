#include "Core/CoreMinimal.h"
#include "RingActor.h"

#include "Engine/Component/Mesh/RingComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h"

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

ARingActor::ARingActor()
{
    auto* RingComponent = new Engine::Component::URingComponent();
    RingComponent->SetColor({ 0.8f,0.8f,0.8f,1.f });
    AddOwnedComponent(RingComponent, true);

    Name = "RingActor";
}

Engine::Component::URingComponent* ARingActor::GetRingComponent() const
{
    return Cast<Engine::Component::URingComponent>(RootComponent);
}

bool ARingActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ARingActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ARingActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ARingActor::GetMeshType() const
{
    if (const auto* RingComponent = GetRingComponent())
    {
        return RingComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ARingActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ARingActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ARingActor)
