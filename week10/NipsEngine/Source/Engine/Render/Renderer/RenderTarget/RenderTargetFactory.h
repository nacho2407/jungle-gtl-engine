#pragma once
#include "RenderTarget.h"

/**
 * 생성 규칙의 중앙 집중화 (파편화된 것보다 생성 규칙 변경 쉬움)
 */
class FRenderTargetFactory
{
  public:
    static FRenderTarget CreateSceneColor(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    static FRenderTarget CreateSceneNormal(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
};
