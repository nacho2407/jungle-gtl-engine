#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"
#include <d3d11.h>

class EditorShadowMapDebugWidget : public FEditorWidget
{
public:
	virtual ~EditorShadowMapDebugWidget() override { ReleaseVizRT(); }
	virtual void Render(float DeltaTime) override;

private:
	// 0=CSM(t21), 1=SpotAtlas(t22), 2=PointAtlas(t23)
	int32 SelectedTab = 0;

	// CSM: 선택된 cascade index + brightness
	int32 CSMCascadeIndex    = 0;
	float CSMDepthBrightness = 1.0f;

	// Spot Atlas: 선택된 page index + 디버그 표시 옵션
	int32 SpotPageIndex       = 0;
	float SpotDepthBrightness = 1.0f;
	bool  bShowSpotRegions    = true;

	// Point Atlas: page index + brightness 조절
	int32 PointPageIndex        = 0;
	float PointDepthBrightness  = 1.0f;
	bool  bShowPointRegions     = true;

	// 시각화 모드: 0 = Linear, 1 = Pow
	int32 VizMode     = 0;
	float VizExponent = 50.0f;

	// ── 시각화 RT (shadow depth → inverted grayscale) ──
	ID3D11Texture2D*          VizTexture = nullptr;
	ID3D11RenderTargetView*   VizRTV     = nullptr;
	ID3D11ShaderResourceView* VizSRV     = nullptr;
	uint32                    VizSize    = 0;
	FConstantBuffer           VizCB;

	void EnsureVizRT(ID3D11Device* Dev, uint32 Size);
	void ReleaseVizRT();
	// Mode: 0 = Linear (1-d)*Brightness, 1 = Pow pow(1-d, Exponent)
	void RenderVizPass(ID3D11DeviceContext* DC, ID3D11ShaderResourceView* SrcSRV,
	                   bool bIsArray, uint32 SliceIndex,
	                   float UVMinX, float UVMinY, float UVMaxX, float UVMaxY,
	                   float Brightness, uint32 Mode = 0, float Exponent = 1.0f);
};
