// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"

/*
    D3D11 텍스처와 그에 연결된 RTV/SRV를 함께 보관하는 저수준 표면 리소스입니다.
    뷰모드 표면, 오프스크린 렌더 타깃 등 여러 중간 표면 리소스에서 재사용합니다.
*/
struct FSurfaceTexture
{
    ID3D11Texture2D*          Texture = nullptr;
    ID3D11RenderTargetView*   RTV     = nullptr;
    ID3D11ShaderResourceView* SRV     = nullptr;
    DXGI_FORMAT               Format  = DXGI_FORMAT_UNKNOWN;
};
