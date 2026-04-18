#include "RenderResources.h"
#include "Materials/MaterialManager.h"
#include "Render/Pipeline/ForwardLightData.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Proxy/FScene.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"

void FSystemResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	LightingConstantBuffer.Create(InDevice, sizeof(FLightingCBData));
	ForwardLights.Create(InDevice, 32);

	RasterizerStateManager.Create(InDevice);
	DepthStencilStateManager.Create(InDevice);
	BlendStateManager.Create(InDevice);
	SamplerStateManager.Create(InDevice);

	FMaterialManager::Get().Initialize(InDevice);
}

void FSystemResources::Release()
{
	SamplerStateManager.Release();
	BlendStateManager.Release();
	DepthStencilStateManager.Release();
	RasterizerStateManager.Release();

	FrameBuffer.Release();
	LightingConstantBuffer.Release();
	ForwardLights.Release();
}

void FSystemResources::UpdateFrameBuffer(ID3D11DeviceContext* Ctx, const FFrameContext& Frame)
{
	FFrameConstants frameConstantData = {};
	frameConstantData.View = Frame.View;
	frameConstantData.Projection = Frame.Proj;
	frameConstantData.InvViewProj = (Frame.View * Frame.Proj).GetInverse();
	frameConstantData.bIsWireframe = (Frame.ViewMode == EViewMode::Wireframe);
	frameConstantData.WireframeColor = Frame.WireframeColor;
	frameConstantData.CameraWorldPos = Frame.CameraPosition;

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	FrameBuffer.Update(Ctx, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = FrameBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Ctx->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}

void FSystemResources::UpdateLightBuffer(ID3D11Device* InDevice, ID3D11DeviceContext* Ctx, const FScene& Scene)
{
	FLightingCBData GlobalLightingData = {};
	if (Scene.HasGlobalAmbientLight())
	{
		FGlobalAmbientLightParams DirLightParams = Scene.GetGlobalAmbientLightParams();
		GlobalLightingData.Ambient.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Ambient.Color = DirLightParams.LightColor;
	}
	else
	{
		// 폴백: 씬에 AmbientLight 없으면 최소 ambient 보장 (검정 방지)
		GlobalLightingData.Ambient.Intensity = 0.15f;
		GlobalLightingData.Ambient.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	if (Scene.HasGlobalDirectionalLight())
	{
		FGlobalDirectionalLightParams DirLightParams = Scene.GetGlobalDirectionalLightParams();
		GlobalLightingData.Directional.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Directional.Color = DirLightParams.LightColor;
		GlobalLightingData.Directional.Direction = DirLightParams.Direction;
	}

	const TArray<FPointLightParams>& PointLightParams = Scene.GetPointLights();
	if (!PointLightParams.empty())
	{
		GlobalLightingData.NumActivePointLights = static_cast<uint32>(PointLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActivePointLights = 0;
	}

	const TArray<FSpotLightParams>& SpotLightParams = Scene.GetSpotLights();
	if (!SpotLightParams.empty())
	{
		GlobalLightingData.NumActiveSpotLights = static_cast<uint32>(SpotLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActiveSpotLights = 0;
	}

	TArray<FLightInfo> Infos;
	for (const FPointLightParams& PointLigth : PointLightParams)
	{
		Infos.emplace_back(PointLigth.ToLightInfo());
	}
	for (const FSpotLightParams& SpotLight : SpotLightParams)
	{
		Infos.emplace_back(SpotLight.ToLightInfo());
	}

	GlobalLightingData.NumTilesX = 0; //똥값. 이후 교체필요
	GlobalLightingData.NumTilesY = 0; //똥값. 이후 교체필요

	LightingConstantBuffer.Update(Ctx, &GlobalLightingData, sizeof(FLightingCBData));
	ID3D11Buffer* b4 = LightingConstantBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);
	Ctx->PSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);

	ForwardLights.Update(InDevice, Ctx, Infos);
	Ctx->VSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
}

void FSystemResources::BindSystemSamplers(ID3D11DeviceContext* Ctx)
{
	SamplerStateManager.BindSystemSamplers(Ctx);
}

void FSystemResources::SetDepthStencilState(ID3D11DeviceContext* Ctx, EDepthStencilState InState)
{
	DepthStencilStateManager.Set(Ctx, InState);
}

void FSystemResources::SetBlendState(ID3D11DeviceContext* Ctx, EBlendState InState)
{
	BlendStateManager.Set(Ctx, InState);
}

void FSystemResources::SetRasterizerState(ID3D11DeviceContext* Ctx, ERasterizerState InState)
{
	RasterizerStateManager.Set(Ctx, InState);
}

void FSystemResources::ResetRenderStateCache()
{
	RasterizerStateManager.ResetCache();
	DepthStencilStateManager.ResetCache();
	BlendStateManager.ResetCache();
}

void FSystemResources::UnbindSystemTextures(ID3D11DeviceContext* Ctx)
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Ctx->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &nullSRV);
}
