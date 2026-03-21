#include "Core/CoreMinimal.h"
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    EPrimitiveType UPrimitiveComponent::GetType() { return {}; }

    void UPrimitiveComponent::SetType(EPrimitiveType NewType) {}

    void UPrimitiveComponent::Update(float DeltaTime) { USceneComponent::Update(DeltaTime); }
    
    REGISTER_CLASS(Engine::Component, UPrimitiveComponent);
} // namespace Engine::Component
