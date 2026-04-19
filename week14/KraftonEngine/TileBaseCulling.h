#pragma once

#include "Source/Engine/Render/Resource/RenderResources.h"
#include "Source/Engine/Render/Pipeline/ForwardLightData.h"
#include "Source/Engine/Render/Pipeline/FrameContext.h"


struct FTileCullingCBData
{
	uint32 ScreenSizeX;
	uint32 ScreenSizeY;
	uint32 Enable25DCulling;
	float  NearZ;
	float  FarZ;
	uint32 NumLights;
	float  _pad[2];
};

class FTileBaseCulling
{
public:
	FTileBaseCulling() = default;
	~FTileBaseCulling() { Release(); }

	FTileBaseCulling(const FTileBaseCulling&) = delete;
	FTileBaseCulling& operator=(const FTileBaseCulling&) = delete;

	void Initialize(ID3D11Device* InDevice);

	void Release();

	bool IsInitialized() const { return bInitialized; }

	void Dispatch(
		ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame,
		ID3D11Buffer*        FrameCB,         // b0 — View/Projection (CS에 별도 바인딩 필요)
		FTileCullingResource& TileCulling,
		ID3D11ShaderResourceView* AllLightsSRV,
		uint32 NumLights,
		uint32 ViewportWidth,
		uint32 ViewportHeight);

private:
	ID3D11Device* Device = nullptr;

	ID3D11ComputeShader* TileLightCullingCS = nullptr;
	ID3D11Buffer*        TileCullingCB      = nullptr; 

	bool bInitialized = false;

	uint32 CachedViewportWidth  = 0;
	uint32 CachedViewportHeight = 0;
};
