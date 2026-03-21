#include "PrimitiveComponent.h"

namespace Engine::Component
{
    const FVector4& UPrimitiveComponent::GetColor() const { return Color; }

    void UPrimitiveComponent::SetColor(const FVector4& NewColor) { Color = NewColor; }

    const Geometry::FAABB& UPrimitiveComponent::GetAABB() const
    {
        if (bBoundsDirty)
        {
            const_cast<UPrimitiveComponent*>(this)->UpdateBounds();
            const_cast<UPrimitiveComponent*>(this)->bBoundsDirty = false;
        }

        return WorldAABB;
    }

    void UPrimitiveComponent::Update(float DeltaTime)
    {
        USceneComponent::Update(DeltaTime);

        if (bBoundsDirty)
        {
            UpdateBounds();
            bBoundsDirty = false;
        }
    }

    void UPrimitiveComponent::UpdateBounds()
    {
        WorldAABB = Geometry::TransformAABB(GetLocalAABB(), GetRelativeMatrix());
    }

    void UPrimitiveComponent::OnTransformChanged() { bBoundsDirty = true; }
} // namespace Engine::Component