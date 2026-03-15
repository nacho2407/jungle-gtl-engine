#pragma once

#include "Component/USceneComponent.h"
#include "World/FScene.h"

class UPrimitiveComponent : public USceneComponent
{
public:
    UPrimitiveComponent();
    virtual ~UPrimitiveComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;

public:
    void SetVisibility(bool bInVisible);
    bool IsVisible() const;

    // UWorld가 Scene 구성할 때 호출
    virtual FRenderItem CreateRenderItem() const;

protected:
    bool bVisible;
};