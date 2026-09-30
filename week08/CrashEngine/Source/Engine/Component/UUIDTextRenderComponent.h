#pragma once

#include "Component/TextRenderComponent.h"

class UUUIDTextRenderComponent : public UTextRenderComponent
{
public:
    DECLARE_CLASS(UUUIDTextRenderComponent, UTextRenderComponent)

    UUUIDTextRenderComponent() = default;
    ~UUUIDTextRenderComponent() override = default;

    void UpdateWorldMatrix() const override;
    FMatrix CalculateBillboardWorldMatrix(const FVector& CameraForward) const override;
    bool UsesUUIDTextShowFlag() const override { return true; }
};
