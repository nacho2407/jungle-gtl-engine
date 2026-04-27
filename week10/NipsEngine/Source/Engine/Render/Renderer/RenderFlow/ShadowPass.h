#pragma once
#include "RenderPass.h"

class FShadowPass : public FBaseRenderPass
{
public:
	bool Initialize() override;
	bool Release() override;

protected:
	bool Begin(const FRenderPassContext* Context) override;
	bool DrawCommand(const FRenderPassContext* Context) override;
	bool End(const FRenderPassContext* Context) override;

private:
    void DrawShadowCaster(const FRenderPassContext* Context, const ULightComponent* Light);
    void DrawShadowCubeCaster(const FRenderPassContext* Context, const ULightComponent* Light);

	// Shadow Atlas에서 라이트별로 할당된 타일 정보를 캐싱하는 맵
    //TMap<ULightComponent*, TArray<FShadowAtlasTile>> ShadowDataCache2D;
    //TMap<ULightComponent*, int32> ShadowDataCacheCube;
};
