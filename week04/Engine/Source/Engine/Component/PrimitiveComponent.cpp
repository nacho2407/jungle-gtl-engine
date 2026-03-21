#include "Core/CoreMinimal.h"
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    EPrimitiveType UPrimitiveComponent::GetType() { return Type; }

    void UPrimitiveComponent::SetType(EPrimitiveType NewType) { Type = NewType; }

    const FVector4& UPrimitiveComponent::GetColor() const { return Color; }

    void UPrimitiveComponent::SetColor(const FVector4& NewColor) { Color = NewColor; }

    void UPrimitiveComponent::Update(float DeltaTime) { USceneComponent::Update(DeltaTime); }

    REGISTER_CLASS(Engine::Component, UPrimitiveComponent);
} // namespace Engine::Component
