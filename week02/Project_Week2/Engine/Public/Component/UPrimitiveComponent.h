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

    // gizmo 축별 색 변경 용이
    void SetRenderColor(const FVector4& InColor);
    const FVector4& GetRenderColor() const;

    // 클릭 애니메이션
    void SetDepthEnable(bool bInDepthEnable);
    void SetCullMode(ERenderCullMode InCullMode);
    void SetUseVertexColor(bool bInUseVertexColor);

protected:
    bool bVisible;
    FVector4 RenderColor = FVector4(1, 1, 1, 1);

    bool bDepthEnable = true;
    bool bUseVertexColor = true; // @@@@ 이름 중복 있음
    ERenderCullMode CullMode = ERenderCullMode::Front;
};