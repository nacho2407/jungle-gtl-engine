#include "RenderTargetBuilder.h"

FRenderTargetBuilder& FRenderTargetBuilder::SetSize(uint32 InWidth, uint32 InHeight)
{
    Width = InWidth;
    Height = InHeight;
    return *this;
}

FRenderTargetBuilder& FRenderTargetBuilder::SetFormat(DXGI_FORMAT InFormat)
{
    Format = InFormat;
    return *this;
}

FRenderTargetBuilder& FRenderTargetBuilder::WithSRV()
{
    bCreateSRV = true;
    return *this;
}

FRenderTargetBuilder& FRenderTargetBuilder::WithRTV()
{
    bCreateRTV = true;
    return *this;
}

FRenderTarget FRenderTargetBuilder::Build(ID3D11Device* Device) 
{
    FRenderTarget RT;

    D3D11_TEXTURE2D_DESC Desc = {};

    Desc.Width = Width;
    Desc.Height = Height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.Format = Format;
    Desc.BindFlags = 0;

    if (bCreateRTV)
        Desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (bCreateSRV)
        Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    Device->CreateTexture2D(&Desc, nullptr, &RT.Texture);

    /** Desc 를 nullptr 로 두면 Texture 설정을 참고하여 자동으로 세팅함 */
    /** 현재는 문제 없는데, 특정 상황에선 직접 세팅해줘야 할 수 있음  */
    if (bCreateRTV)
        Device->CreateRenderTargetView(RT.Texture.Get(), nullptr, &RT.RTV);

    if (bCreateSRV)
        Device->CreateShaderResourceView(RT.Texture.Get(), nullptr, &RT.SRV);

    RT.Width = Width;
    RT.Height = Height;
    RT.Format = Format;

    return RT;
}
