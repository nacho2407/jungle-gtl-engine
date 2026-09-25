#pragma once
#include "Core/CoreMinimal.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/MemoryTracker.h"

#include <dxgiformat.h>

// GPU resource cache 값
struct FTextureResource
{
	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

	TComPtr<ID3D11Texture2D> Texture;
	TComPtr<ID3D11ShaderResourceView> SRV;
    FTrackedTextureAllocationHandle TextureAllocationHandle;

	ID3D11ShaderResourceView* GetSRV() const { return SRV.Get(); }
};
