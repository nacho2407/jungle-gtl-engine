#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11PixelShader;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct FRenderPipelineContext;
class FShadowAtlasPage;

// VSM moment blur와 임시 리소스 생명주기를 담당합니다.
class FShadowMomentFilter
{
public:
    FShadowMomentFilter() = default;
    FShadowMomentFilter(const FShadowMomentFilter&) = delete;
    FShadowMomentFilter& operator=(const FShadowMomentFilter&) = delete;
    ~FShadowMomentFilter();

    void Release();
    void BlurMomentTextureSlice(FRenderPipelineContext& Context, FShadowAtlasPage& AtlasPage, uint32 SliceIndex);

private:
    void EnsureResources(ID3D11Device* Device);

private:
    ID3D11VertexShader*       BlurVS           = nullptr;
    ID3D11PixelShader*        BlurPSHorizontal = nullptr;
    ID3D11PixelShader*        BlurPSVertical   = nullptr;
    ID3D11Buffer*             BlurCB           = nullptr;
    ID3D11Texture2D*          BlurTemp2D       = nullptr;
    ID3D11RenderTargetView*   BlurTempRTV      = nullptr;
    ID3D11ShaderResourceView* BlurTempSRV      = nullptr;
    uint32                    BlurTempSize     = 0;
};
