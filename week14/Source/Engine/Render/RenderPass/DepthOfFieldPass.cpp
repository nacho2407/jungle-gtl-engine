#include "DepthOfFieldPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"

#include <algorithm>
#include <cmath>
#include <cstring>

REGISTER_RENDER_PASS(FDepthOfFieldPass)

namespace
{
	void DrawFullscreenTriangle(ID3D11DeviceContext* DC)
	{
		DC->IASetInputLayout(nullptr);
		DC->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DC->Draw(3, 0);
	}

	uint32 ClampUInt(int32 Value, uint32 MinValue, uint32 MaxValue)
	{
		if (Value < static_cast<int32>(MinValue))
		{
			return MinValue;
		}
		if (Value > static_cast<int32>(MaxValue))
		{
			return MaxValue;
		}
		return static_cast<uint32>(Value);
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	constexpr float WorldUnitToMillimeters = 1000.0f;
	constexpr float MaxForegroundRadiusScreenFraction = 0.018f;
	constexpr float MaxBackgroundRadiusScreenFraction = 0.025f;
	constexpr float ForegroundIntensity = 0.65f;
	constexpr uint32 MaxGatherRingCount = 10u;
	constexpr uint32 MaxGatherSamplesPerRing = 32u;

	struct FCompiledDofSettings
	{
		FCameraDepthOfFieldSettings Settings;
		float CocScalePixels = 0.0f;
		float MaxForegroundRadiusPixels = 0.0f;
		float MaxBackgroundRadiusPixels = 0.0f;
	};

	FCameraDepthOfFieldSettings GetEffectiveDofSettings(const FFrameContext& Frame)
	{
		FCameraDepthOfFieldSettings Settings = Frame.CameraDepthOfField;
		if (Settings.bEnabled)
		{
			return Settings;
		}

		// Non-camera editor viewports can still use the viewport override/debug path.
		if (Frame.RenderOptions.ShowFlags.bDepthOfField)
		{
			Settings.bEnabled = true;
			Settings.FocusDistance = Frame.RenderOptions.DofFocusDistance;
			Settings.FStop = Frame.RenderOptions.DofFStop;
			Settings.SensorWidth = Frame.RenderOptions.DofSensorWidth;
			Settings.GatherRingCount = Frame.RenderOptions.DofGatherRingCount;
			Settings.GatherSamplesPerRing = Frame.RenderOptions.DofGatherSamplesPerRing;
			Settings.bEnableForeground = Frame.RenderOptions.bDofForegroundEnabled;
			Settings.bEnableBackground = Frame.RenderOptions.bDofBackgroundEnabled;
			Settings.bHalfRes = Frame.RenderOptions.bDofHalfRes;
		}
		return Settings;
	}

	float ComputePhysicalCocScalePixels(const FFrameContext& Frame, const FCameraDepthOfFieldSettings& Settings, uint32 SourceWidth)
	{
		const float VerticalFov = ClampFloat(Frame.FOV, 0.01f, 3.13f);
		const float Aspect = (std::max)(Frame.AspectRatio, 0.001f);
		const float SensorWidthMm = (std::max)(Settings.SensorWidth, 0.001f);
		const float FStop = (std::max)(Settings.FStop, 0.001f);
		const float HorizontalFov = 2.0f * std::atan(std::tan(VerticalFov * 0.5f) * Aspect);
		const float HorizontalTanHalf = (std::max)(std::tan(HorizontalFov * 0.5f), 0.0001f);
		const float FocalLengthMm = 0.5f * SensorWidthMm / HorizontalTanHalf;
		const float FocusDistanceMm = (std::max)(Settings.FocusDistance, 0.001f) * WorldUnitToMillimeters;
		const float MinFocusMinusFocalMm = (std::max)(FocalLengthMm * 0.05f, 0.001f);
		const float FocusMinusFocalMm = (std::max)(FocusDistanceMm - FocalLengthMm, MinFocusMinusFocalMm);
		const float InfinityCocDiameterMm = (FocalLengthMm * FocalLengthMm) / (FStop * FocusMinusFocalMm);
		const float CocRadiusNormalized = (InfinityCocDiameterMm * 0.5f) / SensorWidthMm;
		return CocRadiusNormalized * static_cast<float>((std::max)(SourceWidth, 1u));
	}

	FCompiledDofSettings CompileDofSettings(const FFrameContext& Frame, uint32 SourceWidth)
	{
		FCompiledDofSettings Result;
		Result.Settings = GetEffectiveDofSettings(Frame);
		const float SourceWidthFloat = static_cast<float>((std::max)(SourceWidth, 1u));
		Result.CocScalePixels = ComputePhysicalCocScalePixels(Frame, Result.Settings, SourceWidth);
		Result.MaxForegroundRadiusPixels = MaxForegroundRadiusScreenFraction * SourceWidthFloat;
		Result.MaxBackgroundRadiusPixels = MaxBackgroundRadiusScreenFraction * SourceWidthFloat;
		return Result;
	}
}

void FDepthOfFieldPass::FRenderTarget::Release()
{
	if (SRV) { SRV->Release(); SRV = nullptr; }
	if (RTV) { RTV->Release(); RTV = nullptr; }
	if (Texture) { Texture->Release(); Texture = nullptr; }
}

bool FDepthOfFieldPass::FRenderTarget::Create(ID3D11Device* Device, uint32 Width, uint32 Height, const char* DebugName)
{
	Release();
	if (!Device || Width == 0 || Height == 0)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, &Texture);
	if (FAILED(hr))
	{
		Release();
		return false;
	}

	hr = Device->CreateRenderTargetView(Texture, nullptr, &RTV);
	if (FAILED(hr))
	{
		Release();
		return false;
	}

	hr = Device->CreateShaderResourceView(Texture, nullptr, &SRV);
	if (FAILED(hr))
	{
		Release();
		return false;
	}

	if (Texture && DebugName)
	{
		Texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen(DebugName)), DebugName);
	}
	return true;
}

FDepthOfFieldPass::FDepthOfFieldPass()
{
	PassType = ERenderPass::DepthOfField;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
		ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

FDepthOfFieldPass::~FDepthOfFieldPass()
{
	ReleaseResources();
}

bool FDepthOfFieldPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	const FCameraDepthOfFieldSettings DofSettings = GetEffectiveDofSettings(Frame);
	return DofSettings.bEnabled
		&& Frame.ViewportWidth > 0.0f
		&& Frame.ViewportHeight > 0.0f
		&& Frame.SceneColorCopyTexture
		&& Frame.SceneColorCopySRV
		&& Frame.ViewportRenderTexture
		&& Frame.DepthTexture
		&& Frame.DepthCopyTexture
		&& Frame.DepthCopySRV;
}

void FDepthOfFieldPass::Execute(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	ID3D11Device* Device = Ctx.Device.GetDevice();
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	if (!Device || !DC)
	{
		return;
	}

	FShader* SetupShader = FShaderManager::Get().GetOrCreate(
		FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Setup"));
	FShader* GatherShader = FShaderManager::Get().GetOrCreate(
		FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Gather"));
	FShader* RecombineShader = FShaderManager::Get().GetOrCreate(
		FShaderKey(EShaderPath::DepthOfField, nullptr, "VS", "PS_Recombine"));
	if (!SetupShader || !GatherShader || !RecombineShader)
	{
		return;
	}

	const uint32 SourceWidth = static_cast<uint32>((std::max)(Frame.ViewportWidth, 1.0f));
	const uint32 SourceHeight = static_cast<uint32>((std::max)(Frame.ViewportHeight, 1.0f));
	const FCompiledDofSettings CompiledDof = CompileDofSettings(Frame, SourceWidth);
	const FCameraDepthOfFieldSettings& DofSettings = CompiledDof.Settings;
	const bool bHalfRes = DofSettings.bHalfRes;
	const uint32 DofWidth = bHalfRes ? (std::max)(1u, SourceWidth / 2u) : SourceWidth;
	const uint32 DofHeight = bHalfRes ? (std::max)(1u, SourceHeight / 2u) : SourceHeight;

	EnsureResources(Device, DofWidth, DofHeight);
	if (!BackgroundSetupTarget.RTV || !BackgroundSetupTarget.SRV || !ForegroundSetupTarget.RTV || !ForegroundSetupTarget.SRV ||
		!BackgroundTarget.RTV || !BackgroundTarget.SRV || !ForegroundTarget.RTV || !ForegroundTarget.SRV ||
		!ForegroundHoleFillTarget.RTV || !ForegroundHoleFillTarget.SRV)
	{
		return;
	}

	if (!DofCB.GetBuffer())
	{
		DofCB.Create(Device, sizeof(FDepthOfFieldConstants), "DepthOfFieldCB");
	}

	FDepthOfFieldConstants DofData = {};
	const float TargetRadiusScale = SourceWidth > 0 ? static_cast<float>(DofWidth) / static_cast<float>(SourceWidth) : 1.0f;
	const float MaxTargetRadius = (std::max)(CompiledDof.MaxForegroundRadiusPixels, CompiledDof.MaxBackgroundRadiusPixels) * TargetRadiusScale;
	const uint32 RequestedGatherRingCount = ClampUInt(DofSettings.GatherRingCount, 1u, MaxGatherRingCount);
	const uint32 RequestedGatherSamplesPerRing = ClampUInt(DofSettings.GatherSamplesPerRing, 4u, MaxGatherSamplesPerRing);
	const uint32 RadiusDrivenRingCount = ClampUInt(static_cast<int32>(std::ceil(MaxTargetRadius / 4.5f)), 1u, MaxGatherRingCount);
	const uint32 RadiusDrivenSamplesPerRing = ClampUInt(static_cast<int32>(std::ceil(MaxTargetRadius * 0.65f)), 4u, MaxGatherSamplesPerRing);
	const uint32 GatherRingCount = (std::max)(RequestedGatherRingCount, RadiusDrivenRingCount);
	const uint32 GatherSamplesPerRing = (std::max)(RequestedGatherSamplesPerRing, RadiusDrivenSamplesPerRing);
	const float RingSpacing = MaxTargetRadius / (std::max)(static_cast<float>(GatherRingCount), 1.0f);
	const float GatherTransitionWidth = (std::max)(RingSpacing * 0.9f, 1.25f);
	const float GatherSharpness = ClampFloat(1.0f / GatherTransitionWidth, 0.03f, 0.8f);

	DofData.TexelSize = FVector4(
		SourceWidth > 0 ? 1.0f / static_cast<float>(SourceWidth) : 0.0f,
		SourceHeight > 0 ? 1.0f / static_cast<float>(SourceHeight) : 0.0f,
		DofWidth > 0 ? 1.0f / static_cast<float>(DofWidth) : 0.0f,
		DofHeight > 0 ? 1.0f / static_cast<float>(DofHeight) : 0.0f);
	DofData.FocusParams = FVector4(
		(std::max)(DofSettings.FocusDistance, 0.001f),
		(std::max)(CompiledDof.CocScalePixels, 0.0f),
		(std::max)(CompiledDof.MaxForegroundRadiusPixels, 0.0f),
		(std::max)(CompiledDof.MaxBackgroundRadiusPixels, 0.0f));
	DofData.DepthParams = FVector4(
		(std::max)(Frame.NearClip, 0.001f),
		(std::max)(Frame.FarClip, Frame.NearClip + 0.001f),
		TargetRadiusScale,
		GatherSharpness);
	DofData.GatherRingCount = GatherRingCount;
	DofData.GatherSamplesPerRing = GatherSamplesPerRing;
	DofData.bEnableForeground = DofSettings.bEnableForeground ? 1u : 0u;
	DofData.bEnableBackground = DofSettings.bEnableBackground ? 1u : 0u;
	DofData.DebugView = static_cast<uint32>(Frame.RenderOptions.DofDebugView);
	DofData.bHalfRes = bHalfRes ? 1u : 0u;
	DofData.SlightFocusRadius = ClampFloat(0.75f, 0.01f, 4.0f);
	DofData.ForegroundIntensity = ForegroundIntensity;
	DofCB.Update(DC, &DofData, sizeof(DofData));

	ID3D11Buffer* DofCBRaw = DofCB.GetBuffer();
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11RenderTargetView* NullRTV = nullptr;
	ID3D11ShaderResourceView* NullDofSRVs[5] = {};

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	// Preserve the full-resolution HDR scene color and refresh depth for DOF sampling.
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &DofCBRaw);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &DofCBRaw);

	D3D11_VIEWPORT DofViewport = {};
	DofViewport.Width = static_cast<float>(DofWidth);
	DofViewport.Height = static_cast<float>(DofHeight);
	DofViewport.MinDepth = 0.0f;
	DofViewport.MaxDepth = 1.0f;

	D3D11_VIEWPORT SceneViewport = {};
	SceneViewport.Width = Frame.ViewportWidth;
	SceneViewport.Height = Frame.ViewportHeight;
	SceneViewport.MinDepth = 0.0f;
	SceneViewport.MaxDepth = 1.0f;

	// 1. Setup: full-res SceneColor + SceneDepth -> separated background/foreground DOF setup buffers.
	DC->RSSetViewports(1, &DofViewport);
	ID3D11RenderTargetView* SetupRTVs[2] = { BackgroundSetupTarget.RTV, ForegroundSetupTarget.RTV };
	DC->OMSetRenderTargets(2, SetupRTVs, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &Frame.SceneColorCopySRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &Frame.DepthCopySRV);
	SetupShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);

	// 2. Ring gather: separated background, foreground, and foreground hole-fill bokeh accumulations.
	ID3D11RenderTargetView* GatherRTVs[3] = { BackgroundTarget.RTV, ForegroundTarget.RTV, ForegroundHoleFillTarget.RTV };
	const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DC->ClearRenderTargetView(BackgroundTarget.RTV, ClearColor);
	DC->ClearRenderTargetView(ForegroundTarget.RTV, ClearColor);
	DC->ClearRenderTargetView(ForegroundHoleFillTarget.RTV, ClearColor);
	DC->OMSetRenderTargets(3, GatherRTVs, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::DofSetup, 1, &BackgroundSetupTarget.SRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofForegroundSetup, 1, &ForegroundSetupTarget.SRV);
	GatherShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::DofSetup, 2, NullDofSRVs);
	DC->OMSetRenderTargets(1, &NullRTV, nullptr);

	// 3. Recombine: original full-res SceneColor + foreground/background DOF -> viewport SceneColor.
	DC->RSSetViewports(1, &SceneViewport);
	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &Frame.SceneColorCopySRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &Frame.DepthCopySRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofSetup, 1, &BackgroundSetupTarget.SRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofForegroundSetup, 1, &ForegroundSetupTarget.SRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofBackground, 1, &BackgroundTarget.SRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofForeground, 1, &ForegroundTarget.SRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofForegroundHoleFill, 1, &ForegroundHoleFillTarget.SRV);
	RecombineShader->Bind(DC);
	DrawFullscreenTriangle(DC);

	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofSetup, 5, NullDofSRVs);

	ID3D11Buffer* NullCB = nullptr;
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &NullCB);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &NullCB);

	Ctx.Cache.bForceAll = true;
}

void FDepthOfFieldPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRVs[5] = {};
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::DofSetup, 5, NullSRVs);
}

void FDepthOfFieldPass::EnsureResources(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	if (TargetWidth == Width && TargetHeight == Height &&
		BackgroundSetupTarget.Texture && ForegroundSetupTarget.Texture &&
		BackgroundTarget.Texture && ForegroundTarget.Texture && ForegroundHoleFillTarget.Texture)
	{
		return;
	}

	ReleaseResources();
	TargetWidth = Width;
	TargetHeight = Height;

	BackgroundSetupTarget.Create(Device, Width, Height, "DofBackgroundSetupTexture");
	ForegroundSetupTarget.Create(Device, Width, Height, "DofForegroundSetupTexture");
	BackgroundTarget.Create(Device, Width, Height, "DofBackgroundTexture");
	ForegroundTarget.Create(Device, Width, Height, "DofForegroundTexture");
	ForegroundHoleFillTarget.Create(Device, Width, Height, "DofForegroundHoleFillTexture");
}

void FDepthOfFieldPass::ReleaseResources()
{
	BackgroundSetupTarget.Release();
	ForegroundSetupTarget.Release();
	BackgroundTarget.Release();
	ForegroundTarget.Release();
	ForegroundHoleFillTarget.Release();
	DofCB.Release();
	TargetWidth = 0;
	TargetHeight = 0;
}
