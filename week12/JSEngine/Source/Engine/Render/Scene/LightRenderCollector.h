#pragma once

#include "Core/CoreMinimal.h"

class FRenderBus;
class ULightComponentBase;

struct FRenderLightStats
{
    int32 DirectionalLightCount = 0;
    int32 PointLightCount = 0;
    int32 SpotlightCount = 0;
    int32 ShadowCastingLightCount = 0;
    int32 TotalLightCount = 0;
};

class FLightRenderCollector
{
public:
    void Reset();
    void CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus);

    const FRenderLightStats& GetLastStats() const { return LastStats; }

private:
    FRenderLightStats LastStats;
};
