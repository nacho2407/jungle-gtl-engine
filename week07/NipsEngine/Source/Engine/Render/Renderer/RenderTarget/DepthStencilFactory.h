#pragma once
#include "DepthStencilBuilder.h"

class FDepthStencilFactory
{
public:
    static FDepthStencilResource CreateDepthStencilView(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
};
