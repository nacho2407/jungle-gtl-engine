#include "PrimitiveComponent.h"

#include "ComponentProperty.h"
#include "Core/Geometry/Primitives/AABBUtility.h"

namespace Engine::Component
{
    const FColor& UPrimitiveComponent::GetColor() const { return Color; }

    void UPrimitiveComponent::SetColor(const FColor& NewColor) { Color = NewColor; }

    const Geometry::FAABB& UPrimitiveComponent::GetWorldAABB() const
    {
        if (bBoundsDirty)
        {
            const_cast<UPrimitiveComponent*>(this)->UpdateBounds();
            const_cast<UPrimitiveComponent*>(this)->bBoundsDirty = false;
        }

        return WorldAABB;
    }

    bool UPrimitiveComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        return false;
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

    void UPrimitiveComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        USceneComponent::DescribeProperties(Builder);
        Builder.AddColor(
            "color", L"Color", [this]() { return GetColor(); },
            [this](const FColor& InColor) { SetColor(InColor); });
    }

    void UPrimitiveComponent::UpdateBounds()
    {
        WorldAABB = Geometry::TransformAABB(GetLocalAABB(), GetRelativeMatrix());
    }

    void UPrimitiveComponent::OnTransformChanged() { bBoundsDirty = true; }

} // namespace Engine::Component
