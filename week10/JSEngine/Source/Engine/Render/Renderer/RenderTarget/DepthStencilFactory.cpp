#include "DepthStencilFactory.h"

FDepthStencilResource FDepthStencilFactory::CreateDepthStencilView(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    return FDepthStencilBuilder().SetSize(InWidth, InHeight).WithStencil().WithSRV().Build(Device);
}
