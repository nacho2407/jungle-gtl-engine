#include "ShadowMapPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Scene/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Shader/Shader.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/LightFrustumUtils.h"
#include "Profiling/ShadowStats.h"
#include "Core/ProjectSettings.h"
#include "Collision/SpatialPartition.h"
#include "Runtime/Engine.h"
#include "GameFramework/World.h"
#include <algorithm>
#include <d3d11.h>

REGISTER_RENDER_PASS(FShadowMapPass)

// ============================================================
// 생성 / 소멸
// ============================================================

FShadowMapPass::FShadowMapPass()
{
	const auto& ProjShadow = FProjectSettings::Get().Shadow;
	SpotLightAtlas.Init(static_cast<float>(ProjShadow.SpotAtlasResolution), 64.f);
	PointLightAtlas.Init(static_cast<float>(ProjShadow.PointAtlasResolution), 64.f);
	PassType = ERenderPass::ShadowMap;
}

FShadowMapPass::~FShadowMapPass()
{
	ShadowPerObjectCB.Release();
	ShadowLightCB.Release();
}

// ============================================================
// SetupShadowRenderState — SRV 언바인딩 + 공용 렌더 상태
// ============================================================

void FShadowMapPass::SetupShadowRenderState(FD3DDevice& Device, FSystemResources& Resources, ID3D11DeviceContext* DC)
{
	// 이전 프레임 Shadow SRV 언바인딩 (DSV/RTV와 동일 리소스 → R/W hazard 방지)
	ID3D11ShaderResourceView* nullSRVs[5] = {};
	DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 3, nullSRVs);       // t21~t23
	DC->PSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 2, nullSRVs);    // t24~t25
	DC->VSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 3, nullSRVs);       // t21~t23
	DC->VSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 2, nullSRVs);    // t24~t25

	// FilterMode 결정
	CurrentFilterMode = FShadowSettings::Get().GetEffectiveFilterMode();

	// CB 생성 (한 번만)
	ID3D11Device* Dev = Device.GetDevice();
	if (!ShadowPerObjectCB.GetBuffer())
		ShadowPerObjectCB.Create(Dev, sizeof(FPerObjectConstants));
	if (!ShadowLightCB.GetBuffer())
		ShadowLightCB.Create(Dev, sizeof(FMatrix));

	// ImGui 등 외부 코드가 D3D state를 직접 변경할 수 있으므로 캐시 무효화
	Resources.ResetRenderStateCache();

	// 공용 렌더 상태 세팅 (Reversed-Z: GREATER_EQUAL — 엔진 컨벤션 통일)
	Resources.SetDepthStencilState(Device, EDepthStencilState::Default);
	Resources.SetRasterizerState(Device, ERasterizerState::SolidFrontCull);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (CurrentFilterMode == EShadowFilterMode::VSM)
		Resources.SetBlendState(Device, EBlendState::Opaque);
	else
		Resources.SetBlendState(Device, EBlendState::NoColor);

	// Shadow CB 캐시 초기화 — Bias/SlopeBias/Sharpen은 RenderDirectionalShadows에서 per-light 값으로 갱신
	ShadowCBCache = {};
	ShadowCBCache.ShadowBias       = FShadowSettings::kDefaultBias;
	ShadowCBCache.ShadowSlopeBias  = FShadowSettings::kDefaultSlopeBias;
	ShadowCBCache.ShadowFilterMode = static_cast<uint32>(CurrentFilterMode);
	ShadowCBCache.CSMBlendRange    = FShadowSettings::kDefaultCSMBlendRange;
	ShadowCBCache.CSMBlendEnabled  = FShadowSettings::kDefaultCSMBlendEnabled ? 1u : 0u;
}

// ============================================================
// BeginPass
// ============================================================

bool FShadowMapPass::BeginPass(const FPassContext& Ctx)
{
	const auto& Shadow = FProjectSettings::Get().Shadow;
	if (!Shadow.bEnabled)
	{
		FShadowMapResources& Res = Ctx.Resources.ShadowResources;
		if (Res.CSM.IsValid() || Res.Spot.IsValid() || Res.Point.IsValid())
		{
			// GPU 리소스 해제
			Res.Release();

			// Shadow SRV 언바인드 (해제된 텍스처가 바인딩에 남지 않도록)
			ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
			ID3D11ShaderResourceView* nullSRVs[5] = {};
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 5, nullSRVs);

			// Shadow CB (b5) 초기화 → 셰이더에서 NumCSMCascades=0 등으로 early-out
			ShadowCBCache = {};
			Ctx.Resources.ShadowConstantBuffer.Update(DC, &ShadowCBCache, sizeof(FShadowCBData));
			ID3D11Buffer* b5 = Ctx.Resources.ShadowConstantBuffer.GetBuffer();
			DC->PSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
		}

		// Stats 초기화
		SHADOW_STATS_RESET();
		SHADOW_STATS_SET_MEMORY(0);
		SHADOW_STATS_SET_RESOLUTION(0);

		return false;
	}

	SetupShadowRenderState(Ctx.Device, Ctx.Resources, Ctx.Device.GetDeviceContext());
	EnsureResources(Ctx);
	return true;
}

// ============================================================
// Execute — 라이트 타입별 Shadow 렌더링
// ============================================================

void FShadowMapPass::Execute(const FPassContext& Ctx)
{
	if (!Ctx.Scene) return;

	SHADOW_STATS_RESET();

	FShadowMapResources& ShadowRes = Ctx.Resources.ShadowResources;

	// CSM은 카메라 프러스텀 기반 → 뷰포트마다 재렌더링
	RenderDirectionalShadows(Ctx, ShadowRes);

	// Spot/Point는 라이트 시점 → 프레임당 1회만 렌더링
	if (ShadowRes.FrameGeneration != LastRenderedGeneration)
	{
		RenderSpotShadows(Ctx, ShadowRes);
		RenderPointShadows(Ctx, ShadowRes);
		LastRenderedGeneration = ShadowRes.FrameGeneration;
	}

	// VSM blur pass — shadow depth 렌더 완료 후 moment 텍스처에 Gaussian blur 적용
	if (CurrentFilterMode == EShadowFilterMode::VSM)
	{
		//BlurVSMTexture(Ctx, ShadowRes);
	}
}

// ============================================================
// EndPass — 메인 RT 복원, Shadow SRV 바인딩, Shadow CB 업데이트
// ============================================================

void FShadowMapPass::EndPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShadowMapResources& ShadowRes = Ctx.Resources.ShadowResources;

	// 메인 RT/DSV 복원
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidBackCull);
	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	Ctx.Cache.bForceAll = true;

	// 카메라 View/Proj를 b0에 복원
	Ctx.Resources.UpdateFrameBuffer(Ctx.Device, Ctx.Frame);

	// 메인 뷰포트 복원
	D3D11_VIEWPORT MainVP = {};
	MainVP.Width    = Ctx.Frame.ViewportWidth;
	MainVP.Height   = Ctx.Frame.ViewportHeight;
	MainVP.MinDepth = 0.0f;
	MainVP.MaxDepth = 1.0f;
	DC->RSSetViewports(1, &MainVP);

	BindShadowSRVs(DC, ShadowRes);
	UpdateShadowCB(Ctx);
	PatchLightBuffer(Ctx);
	UpdateShadowStats(ShadowRes);
}

// ============================================================
// PatchLightBuffer — ShadowMapIndex / bCastShadow 갱신
// ============================================================

void FShadowMapPass::PatchLightBuffer(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumPoints = Env.GetNumPointLights();
	const uint32 NumSpots  = Env.GetNumSpotLights();

	ID3D11Buffer* LightBuf = Ctx.Resources.ForwardLights.LightBuffer;
	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	HRESULT hr = DC->Map(LightBuf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &Mapped);
	if (FAILED(hr)) return;

	FLightInfo* Lights = static_cast<FLightInfo*>(Mapped.pData);

	// Point lights: index [0, NumPoints)
	for (uint32 i = 0; i < NumPoints; ++i)
	{
		if (Lights[i].bCastShadow)
		{
			int32 ShadowIdx = (i < static_cast<uint32>(PointShadowIndexMap.size())) ? PointShadowIndexMap[i] : -1;
			if (ShadowIdx >= 0)
				Lights[i].ShadowMapIndex = static_cast<uint32>(ShadowIdx);
			else
				Lights[i].bCastShadow = 0;
		}
	}

	// Spot lights: index [NumPoints, NumPoints + NumSpots)
	for (uint32 i = 0; i < NumSpots; ++i)
	{
		FLightInfo& L = Lights[NumPoints + i];
		if (L.bCastShadow)
		{
			int32 ShadowIdx = (i < static_cast<uint32>(SpotShadowIndexMap.size())) ? SpotShadowIndexMap[i] : -1;
			if (ShadowIdx >= 0)
				L.ShadowMapIndex = static_cast<uint32>(ShadowIdx);
			else
				L.bCastShadow = 0;
		}
	}

	DC->Unmap(LightBuf, 0);
}

// ============================================================
// UpdateShadowStats — Shadow 해상도 + 메모리 통계
// ============================================================

void FShadowMapPass::UpdateShadowStats(const FShadowMapResources& Res)
{
	SHADOW_STATS_SET_RESOLUTION(Res.CSM.Resolution);

	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);
	// depth(4B) + moment(8B) + blur_temp(8B) per texel when VSM
	const uint32 BPP = bVSM ? 4 + 8 + 8 : 4;
	uint64 TotalBytes = 0;

	if (Res.CSM.Resolution > 0)
		TotalBytes += static_cast<uint64>(Res.CSM.Resolution) * Res.CSM.Resolution * MAX_SHADOW_CASCADES * BPP;

	if (Res.Spot.PageCount > 0)
		TotalBytes += static_cast<uint64>(Res.Spot.Resolution) * Res.Spot.Resolution * Res.Spot.PageCount * BPP;

	if (Res.Point.PageCount > 0)
		TotalBytes += static_cast<uint64>(Res.Point.Resolution) * Res.Point.Resolution * Res.Point.PageCount * BPP;

	SHADOW_STATS_SET_MEMORY(TotalBytes);
}

// ============================================================
// BindShadowSRVs — Shadow SRV 바인딩
// ============================================================

void FShadowMapPass::BindShadowSRVs(ID3D11DeviceContext* DC, FShadowMapResources& Res)
{
	if (CurrentFilterMode == EShadowFilterMode::VSM)
	{
		// VSM: moment texture SRV 바인딩 (R32G32_FLOAT)
		if (Res.CSM.VSMSRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.VSMSRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.VSMSRV);
		}
		else if (Res.CSM.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.SRV);
		}

		if (Res.Spot.VSMSRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.VSMSRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.VSMSRV);
		}
		else if (Res.Spot.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.SRV);
		}

		if (Res.Point.VSMSRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.VSMSRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.VSMSRV);
		}
		else if (Res.Point.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.SRV);
		}
	}
	else
	{
		// Hard/PCF: depth SRV 바인딩 (R32_FLOAT)
		if (Res.CSM.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSM.SRV);
		}
		if (Res.Spot.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.Spot.SRV);
		}
		if (Res.Point.SRV)
		{
			DC->PSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.SRV);
			DC->VSSetShaderResources(ESystemTexSlot::ShadowMapPointLightTextureArray, 1, &Res.Point.SRV);
		}
	}

	if (Res.Spot.DataSRV)
	{
		DC->PSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 1, &Res.Spot.DataSRV);
		DC->VSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 1, &Res.Spot.DataSRV);
	}
	if (Res.Point.DataSRV)
	{
		DC->PSSetShaderResources(ESystemTexSlot::PointShadowDatas, 1, &Res.Point.DataSRV);
		DC->VSSetShaderResources(ESystemTexSlot::PointShadowDatas, 1, &Res.Point.DataSRV);
	}
}

// ============================================================
// UploadLightViewProj — b2 (PerShader0)에 LightViewProj 업로드
// ============================================================

void FShadowMapPass::UploadLightViewProj(ID3D11DeviceContext* DC, const FMatrix& LightViewProj)
{
	ShadowLightCB.Update(DC, &LightViewProj, sizeof(FMatrix));
	ID3D11Buffer* b2 = ShadowLightCB.GetBuffer();
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &b2);
}

// ============================================================
// EnsureResources — 모든 라이트 타입의 리소스를 일괄 Ensure
// ============================================================

void FShadowMapPass::EnsureResources(const FPassContext& Ctx)
{
	ID3D11Device* Dev = Ctx.Device.GetDevice();
	FShadowMapResources& Res = Ctx.Resources.ShadowResources;
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const auto& ProjShadow = FProjectSettings::Get().Shadow;
	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);

	// VSM → Non-VSM 전환 시 VSM 전용 리소스 해제
	if (!bVSM)
	{
		if (Res.CSM.IsVSMValid())   Res.CSM.ReleaseVSM();
		if (Res.Spot.IsVSMValid())  Res.Spot.ReleaseVSM();
		if (Res.Point.IsVSMValid()) Res.Point.ReleaseVSM();
	}

	// Atlas 해상도가 변경되었으면 재초기화
	if (static_cast<uint32>(SpotLightAtlas.GetAtlasSize()) != ProjShadow.SpotAtlasResolution)
	{
		SpotLightAtlas.Clear();
		SpotLightAtlas.Init(static_cast<float>(ProjShadow.SpotAtlasResolution), 64.f);
	}
	if (static_cast<uint32>(PointLightAtlas.GetAtlasSize()) != ProjShadow.PointAtlasResolution)
	{
		PointLightAtlas.Clear();
		PointLightAtlas.Init(static_cast<float>(ProjShadow.PointAtlasResolution), 64.f);
	}

	// ── CSM (Directional) — Directional Light가 있을 때만 생성, 없으면 해제 ──
	if (Env.HasGlobalDirectionalLight())
	{
		const FGlobalDirectionalLightParams& DirectionalParams = Env.GetGlobalDirectionalLightParams();
		const float ScaledResolution = static_cast<float>(ProjShadow.CSMResolution) * DirectionalParams.ShadowResolutionScale;
		uint32 Resolution = static_cast<uint32>((std::max)(64.0f, (std::min)(ScaledResolution, 8192.0f)));
		
		Res.EnsureCSM(Dev, Resolution);
		if (bVSM) Res.EnsureCSM_VSM(Dev, Resolution);
	}
	else if (Res.CSM.IsValid())
	{
		Res.CSM.Release();
	}

	// ── 공용 카메라 파라미터 ──
	const FConvexVolume& CameraFrustum = Ctx.Frame.FrustumVolume;
	const float FOVy = 2.0f * atanf(1.0f / Ctx.Frame.Proj.M[1][1]);
	constexpr float PackingOverhead = 1.3f;

	// ── Spot Atlas — 컬링 + 페이지 추정 + 사전 분배 ──
	VisibleShadowSpotIndices.clear();
	SpotPageGroups.clear();
	const uint32 NumSpots = Env.GetNumSpotLights();
	for (uint32 i = 0; i < NumSpots; ++i)
	{
		const auto& Light = Env.GetSpotLight(i);
		if (!Light.bCastShadows) continue;
		if (!CameraFrustum.IntersectSphere(Light.Position, Light.AttenuationRadius)) continue;
		VisibleShadowSpotIndices.push_back(i);
		if (VisibleShadowSpotIndices.size() >= MAX_SHADOW_SPOT_LIGHTS) break;
	}
	const uint32 ShadowSpotCount = static_cast<uint32>(VisibleShadowSpotIndices.size());

	if (ShadowSpotCount > 0)
	{
		const uint32 SpotRes = static_cast<uint32>(SpotLightAtlas.GetAtlasSize());
		const uint32 MaxSpotPages = FProjectSettings::Get().Shadow.MaxSpotAtlasPages;
		const float SpotAtlasArea = static_cast<float>(SpotRes) * static_cast<float>(SpotRes);

		// ComputeSnappedResolution으로 각 라이트의 해상도 산출
		struct FAlloc { uint32 visIdx; uint32 snappedRes; };
		TArray<FAlloc> SpotAllocs(ShadowSpotCount);
		float TotalSpotArea = 0.0f;
		for (uint32 i = 0; i < ShadowSpotCount; ++i)
		{
			uint32 snapped = SpotLightAtlas.ComputeSnappedResolution(
				Env.GetSpotLight(VisibleShadowSpotIndices[i]),
				Ctx.Frame.CameraPosition, Ctx.Frame.CameraForward, FOVy, Ctx.Frame.ViewportHeight);
			SpotAllocs[i] = { i, snapped };
			TotalSpotArea += static_cast<float>(snapped) * static_cast<float>(snapped);
		}

		// 페이지 수 추정 — MaxPages 초과 시 해상도 비례 축소
		uint32 EstimatedSpotPages = static_cast<uint32>(ceilf(TotalSpotArea * PackingOverhead / SpotAtlasArea));
		if (EstimatedSpotPages > MaxSpotPages)
		{
			float AvailableArea = static_cast<float>(MaxSpotPages) * SpotAtlasArea;
			float Scale = sqrtf(AvailableArea / (TotalSpotArea * PackingOverhead));
			TotalSpotArea = 0.0f;
			for (uint32 i = 0; i < ShadowSpotCount; ++i)
			{
				uint32 scaled = SpotLightAtlas.RoundToNearestPowerOfTwo(
					static_cast<uint32>(static_cast<float>(SpotAllocs[i].snappedRes) * Scale));
				scaled = (std::max)(scaled, static_cast<uint32>(SpotLightAtlas.GetMinResolution()));
				SpotAllocs[i].snappedRes = scaled;
				TotalSpotArea += static_cast<float>(scaled) * static_cast<float>(scaled);
			}
			EstimatedSpotPages = static_cast<uint32>(ceilf(TotalSpotArea * PackingOverhead / SpotAtlasArea));
		}
		EstimatedSpotPages = (std::max)(1u, (std::min)(EstimatedSpotPages, MaxSpotPages));

		Res.EnsureSpotAtlas(Dev, SpotRes, EstimatedSpotPages, ShadowSpotCount);
		if (bVSM) Res.EnsureSpotAtlas_VSM(Dev, SpotRes, EstimatedSpotPages);

		// 축소된 해상도 저장 (visIdx 기준 — RenderSpotShadows에서 사용)
		SpotScaledResolutions.resize(ShadowSpotCount);
		for (uint32 i = 0; i < ShadowSpotCount; ++i)
			SpotScaledResolutions[SpotAllocs[i].visIdx] = SpotAllocs[i].snappedRes;

		// 해상도 내림차순 정렬 → area-budget 기반 페이지 분배
		std::sort(SpotAllocs.begin(), SpotAllocs.end(), [](const FAlloc& a, const FAlloc& b) {
			return a.snappedRes > b.snappedRes;
		});

		const float SpotPageBudget = SpotAtlasArea / PackingOverhead;
		SpotPageGroups.resize(EstimatedSpotPages);
		float spotUsed = 0.0f;
		uint32 spotPage = 0;
		for (const auto& alloc : SpotAllocs)
		{
			float area = static_cast<float>(alloc.snappedRes) * static_cast<float>(alloc.snappedRes);
			if (spotUsed > 0.0f && spotUsed + area > SpotPageBudget && spotPage + 1 < EstimatedSpotPages)
			{
				++spotPage;
				spotUsed = 0.0f;
			}
			SpotPageGroups[spotPage].push_back(alloc.visIdx);
			spotUsed += area;
		}
	}
	else if (Res.Spot.IsValid())
	{
		Res.Spot.Release();
	}

	// ── Point Atlas — 컬링 + 페이지 추정 + 사전 분배 ──
	VisibleShadowPointIndices.clear();
	PointPageGroups.clear();
	const uint32 NumPoints = Env.GetNumPointLights();
	for (uint32 i = 0; i < NumPoints; ++i)
	{
		const auto& Light = Env.GetPointLight(i);
		if (!Light.bCastShadows) continue;
		if (!CameraFrustum.IntersectSphere(Light.Position, Light.AttenuationRadius * 2.0f)) continue;
		VisibleShadowPointIndices.push_back(i);
		if (VisibleShadowPointIndices.size() >= MAX_SHADOW_POINT_LIGHTS) break;
	}
	const uint32 ShadowPointCount = static_cast<uint32>(VisibleShadowPointIndices.size());

	if (ShadowPointCount > 0)
	{
		const uint32 PointAtlasSize = static_cast<uint32>(PointLightAtlas.GetAtlasSize());
		const uint32 MaxPointPages = FProjectSettings::Get().Shadow.MaxPointAtlasPages;
		const float PointAtlasArea = static_cast<float>(PointAtlasSize) * static_cast<float>(PointAtlasSize);

		// ComputeSnappedResolution으로 각 라이트의 해상도 산출
		struct FAlloc { uint32 visIdx; uint32 snappedRes; };
		TArray<FAlloc> PointAllocs(ShadowPointCount);
		float TotalFaceArea = 0.0f;
		for (uint32 i = 0; i < ShadowPointCount; ++i)
		{
			uint32 snapped = PointLightAtlas.ComputeSnappedResolution(
				Env.GetPointLight(VisibleShadowPointIndices[i]),
				Ctx.Frame.CameraPosition, Ctx.Frame.CameraForward, FOVy, Ctx.Frame.ViewportHeight);
			PointAllocs[i] = { i, snapped };
			TotalFaceArea += 6.0f * static_cast<float>(snapped) * static_cast<float>(snapped);
		}

		// 페이지 수 추정 — MaxPages 초과 시 해상도 비례 축소
		uint32 EstimatedPages = static_cast<uint32>(ceilf(TotalFaceArea * PackingOverhead / PointAtlasArea));
		if (EstimatedPages > MaxPointPages)
		{
			float AvailableArea = static_cast<float>(MaxPointPages) * PointAtlasArea;
			float Scale = sqrtf(AvailableArea / (TotalFaceArea * PackingOverhead));
			TotalFaceArea = 0.0f;
			for (uint32 i = 0; i < ShadowPointCount; ++i)
			{
				uint32 scaled = PointLightAtlas.RoundToNearestPowerOfTwo(
					static_cast<uint32>(static_cast<float>(PointAllocs[i].snappedRes) * Scale));
				scaled = (std::max)(scaled, static_cast<uint32>(PointLightAtlas.GetMinResolution()));
				PointAllocs[i].snappedRes = scaled;
				TotalFaceArea += 6.0f * static_cast<float>(scaled) * static_cast<float>(scaled);
			}
			EstimatedPages = static_cast<uint32>(ceilf(TotalFaceArea * PackingOverhead / PointAtlasArea));
		}
		EstimatedPages = (std::max)(1u, (std::min)(EstimatedPages, MaxPointPages));

		Res.EnsurePointAtlas(Dev, PointAtlasSize, EstimatedPages, ShadowPointCount);
		if (bVSM) Res.EnsurePointAtlas_VSM(Dev, PointAtlasSize, EstimatedPages);

		// 축소된 해상도 저장 (visIdx 기준 — RenderPointShadows에서 사용)
		PointScaledResolutions.resize(ShadowPointCount);
		for (uint32 i = 0; i < ShadowPointCount; ++i)
			PointScaledResolutions[PointAllocs[i].visIdx] = PointAllocs[i].snappedRes;

		// 해상도 내림차순 정렬 → area-budget 기반 페이지 분배
		std::sort(PointAllocs.begin(), PointAllocs.end(), [](const FAlloc& a, const FAlloc& b) {
			return a.snappedRes > b.snappedRes;
		});

		const float PointPageBudget = PointAtlasArea / PackingOverhead;
		PointPageGroups.resize(EstimatedPages);
		float pointUsed = 0.0f;
		uint32 pointPage = 0;
		for (const auto& alloc : PointAllocs)
		{
			// 6 faces per light
			float area = 6.0f * static_cast<float>(alloc.snappedRes) * static_cast<float>(alloc.snappedRes);
			if (pointUsed > 0.0f && pointUsed + area > PointPageBudget && pointPage + 1 < EstimatedPages)
			{
				++pointPage;
				pointUsed = 0.0f;
			}
			PointPageGroups[pointPage].push_back(alloc.visIdx);
			pointUsed += area;
		}
	}
	else if (Res.Point.IsValid())
	{
		Res.Point.Release();
	}
}

// ============================================================
// UpdateShadowCB — Shadow CB (b5) 데이터 조립 + GPU 업로드
// ============================================================

void FShadowMapPass::UpdateShadowCB(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShadowMapResources& Res = Ctx.Resources.ShadowResources;

	ShadowCBCache.CSMResolution = Res.CSM.Resolution;
	ShadowCBCache.SpotAtlasResolution  = Res.Spot.Resolution > 0 ? Res.Spot.Resolution : 4096;
	ShadowCBCache.PointAtlasResolution = Res.Point.Resolution > 0 ? Res.Point.Resolution : 4096;

	Ctx.Resources.ShadowConstantBuffer.Update(DC, &ShadowCBCache, sizeof(FShadowCBData));
	ID3D11Buffer* b5 = Ctx.Resources.ShadowConstantBuffer.GetBuffer();
	DC->PSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
	DC->VSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
}

// ============================================================
// DrawShadowCasters — 공용 프록시 순회 + depth-only 렌더링
// ============================================================

void FShadowMapPass::DrawShadowCasters(ID3D11DeviceContext* DC, FScene& Scene, FSystemResources& Resources, const FConvexVolume& LightFrustum, FSpatialPartition* Partition)
{
	FShader* ShadowShader = FShaderManager::Get().GetOrCreate(EShaderPath::ShadowDepth);
	if (!ShadowShader || !ShadowShader->IsValid()) return;

	ShadowShader->Bind(DC);

	if (CurrentFilterMode != EShadowFilterMode::VSM)
		DC->PSSetShader(nullptr, nullptr, 0);

	TArray<FPrimitiveSceneProxy*> BroadPhaseProxies;
	const TArray<FPrimitiveSceneProxy*>* ProxyList = nullptr;

	if (Partition)
	{
		Partition->QueryFrustumAllProxies(LightFrustum, BroadPhaseProxies);
		ProxyList = &BroadPhaseProxies;
	}
	else
	{
		ProxyList = &Scene.GetAllProxies();
	}

	LastDrawCasterCount = 0;
	bool bCurrentTwoSided = false;
	for (FPrimitiveSceneProxy* Proxy : *ProxyList)
	{
		if (!Proxy || !Proxy->IsVisible()) continue;
		if (!Proxy->CastsShadow()) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull)) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly)) continue;

		if (!Partition && !LightFrustum.IntersectAABB(Proxy->GetCachedBounds())) continue;

		FMeshBuffer* Mesh = Proxy->GetMeshBuffer();
		if (!Mesh || !Mesh->IsValid()) continue;

		// Two-sided shadow: front-cull ↔ no-cull 전환
		bool bTwoSided = Proxy->CastsShadowAsTwoSided();
		if (bTwoSided != bCurrentTwoSided)
		{
			bCurrentTwoSided = bTwoSided;
			Resources.RasterizerStateManager.Set(DC,
				bTwoSided ? ERasterizerState::SolidNoCull : ERasterizerState::SolidFrontCull);
		}

		++LastDrawCasterCount;
		ShadowPerObjectCB.Update(DC, &Proxy->GetPerObjectConstants(), sizeof(FPerObjectConstants));
		ID3D11Buffer* b1 = ShadowPerObjectCB.GetBuffer();
		DC->VSSetConstantBuffers(ECBSlot::PerObject, 1, &b1);

		ID3D11Buffer* VB = Mesh->GetVertexBuffer().GetBuffer();
		uint32 VBStride = Mesh->GetVertexBuffer().GetStride();
		uint32 Offset = 0;
		DC->IASetVertexBuffers(0, 1, &VB, &VBStride, &Offset);

		ID3D11Buffer* IB = Mesh->GetIndexBuffer().GetBuffer();
		if (IB)
			DC->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

		for (const FMeshSectionDraw& Section : Proxy->GetSectionDraws())
		{
			if (Section.IndexCount == 0) continue;
			DC->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
			SHADOW_STATS_ADD_DRAW_CALL();
		}
	}
	// Front-cull로 복원
	if (bCurrentTwoSided)
		Resources.RasterizerStateManager.Set(DC, ERasterizerState::SolidFrontCull);
}

void FShadowMapPass::DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum)
{
	FSpatialPartition* Partition = nullptr;
	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorld())
			Partition = &World->GetPartition();
	}
	DrawShadowCasters(Ctx.Device.GetDeviceContext(), *Ctx.Scene, Ctx.Resources, LightFrustum, Partition);
}

// ============================================================
// BlurVSMTexture — VSM moment 텍스처에 separable Gaussian blur 적용
// ============================================================
// 각 라이트 타입(CSM/Spot/Point)의 moment 텍스처에 대해:
//   Pass 1: Source SRV → Horizontal blur → Temp RTV
//   Pass 2: Temp SRV   → Vertical blur   → Source RTV
// ShadowSharpen 값으로 blur 반경 결정: sharpen 0=max blur(3), sharpen 1=no blur(0)

void FShadowMapPass::BlurVSMTexture(const FPassContext& Ctx, FShadowMapResources& Res)
{
	FShader* BlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::VSMBlur);
	if (!BlurShader || !BlurShader->IsValid()) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// Save current state — we'll restore after blur
	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	BlurShader->Bind(DC);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// RTV/SRV unbind helpers
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11RenderTargetView*   nullRTV = nullptr;

	// Shadow depth 렌더 후 남아있는 RTV 바인딩 해제 (R/W hazard 방지)
	DC->OMSetRenderTargets(1, &nullRTV, nullptr);

	// Lambda: blur a single Texture2DArray
	auto BlurTextureArray = [&](
		ID3D11ShaderResourceView* SrcSRV,
		ID3D11RenderTargetView* const* TempRTVs,
		ID3D11ShaderResourceView* TempSRV,
		ID3D11RenderTargetView* const* DstRTVs,
		uint32 Resolution, uint32 SliceCount, float BlurRadius)
	{
		if (!SrcSRV || !TempSRV || BlurRadius <= 0.0f) return;

		float TexelSize = 1.0f / static_cast<float>(Resolution);

		D3D11_VIEWPORT VP = {};
		VP.Width    = static_cast<float>(Resolution);
		VP.Height   = static_cast<float>(Resolution);
		VP.MinDepth = 0.0f;
		VP.MaxDepth = 1.0f;
		DC->RSSetViewports(1, &VP);

		for (uint32 Slice = 0; Slice < SliceCount; ++Slice)
		{
			FVSMBlurCBData CBData;
			CBData.ArraySlice = static_cast<float>(Slice);
			CBData.BlurRadius = BlurRadius;

			// Pass 1: Horizontal — Source → Temp
			CBData.TexelDirX  = TexelSize;
			CBData.TexelDirY  = 0.0f;

			ShadowLightCB.Update(DC, &CBData, sizeof(FVSMBlurCBData));
			ID3D11Buffer* b2 = ShadowLightCB.GetBuffer();
			DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &b2);
			DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &b2);

			DC->OMSetRenderTargets(1, &TempRTVs[Slice], nullptr);
			DC->PSSetShaderResources(0, 1, &SrcSRV);
			DC->Draw(3, 0);

			// Unbind RTV + SRV before pass 2 (Temp is now both src and was dst)
			DC->OMSetRenderTargets(1, &nullRTV, nullptr);
			DC->PSSetShaderResources(0, 1, &nullSRV);

			// Pass 2: Vertical — Temp → Dest (original moment texture)
			CBData.TexelDirX  = 0.0f;
			CBData.TexelDirY  = TexelSize;

			ShadowLightCB.Update(DC, &CBData, sizeof(FVSMBlurCBData));

			DC->OMSetRenderTargets(1, &DstRTVs[Slice], nullptr);
			DC->PSSetShaderResources(0, 1, &TempSRV);
			DC->Draw(3, 0);

			// Unbind both for next slice (DstRTV[Slice] shares texture with SrcSRV)
			DC->OMSetRenderTargets(1, &nullRTV, nullptr);
			DC->PSSetShaderResources(0, 1, &nullSRV);
		}
	};

	// Directional CSM 기본 sharpen (전체 CSM에 공통 적용)
	float DirectionalSharpen = ShadowCBCache.ShadowSharpen;
	float CSMBlurRadius = std::round((1.0f - DirectionalSharpen) * 3.0f);

	// ── CSM Blur ──
	if (Res.CSM.IsVSMValid() && Res.CSM.VSMBlurTemp)
	{
		BlurTextureArray(
			Res.CSM.VSMSRV,
			Res.CSM.VSMBlurTempRTV,
			Res.CSM.VSMBlurTempSRV,
			Res.CSM.VSMRTV,
			Res.CSM.Resolution, MAX_SHADOW_CASCADES, CSMBlurRadius);
	}

	// ── Spot Blur ──
	if (Res.Spot.IsVSMValid() && Res.Spot.VSMBlurTemp && Res.Spot.PageCount > 0)
	{
		// Spot은 per-light sharpen이 다를 수 있지만, atlas 단위로 blur하므로
		// 전체 페이지에 공통 blur 적용 (가장 soft한 라이트 기준)
		float MinSharpen = 1.0f;
		for (uint32 i = 0; i < VisibleShadowSpotIndices.size(); ++i)
		{
			const auto& Light = Ctx.Scene->GetEnvironment().GetSpotLight(VisibleShadowSpotIndices[i]);
			MinSharpen = (std::min)(MinSharpen, Light.ShadowSharpen);
		}
		float SpotBlurRadius = std::round((1.0f - MinSharpen) * 3.0f);

		BlurTextureArray(
			Res.Spot.VSMSRV,
			Res.Spot.VSMBlurTempRTVs.data(),
			Res.Spot.VSMBlurTempSRV,
			Res.Spot.VSMRTVs.data(),
			Res.Spot.Resolution, Res.Spot.PageCount, SpotBlurRadius);
	}

	// ── Point Blur ──
	if (Res.Point.IsVSMValid() && Res.Point.VSMBlurTemp && Res.Point.PageCount > 0)
	{
		float MinSharpen = 1.0f;
		for (uint32 i = 0; i < VisibleShadowPointIndices.size(); ++i)
		{
			const auto& Light = Ctx.Scene->GetEnvironment().GetPointLight(VisibleShadowPointIndices[i]);
			MinSharpen = (std::min)(MinSharpen, Light.ShadowSharpen);
		}
		float PointBlurRadius = std::round((1.0f - MinSharpen) * 3.0f);

		BlurTextureArray(
			Res.Point.VSMSRV,
			Res.Point.VSMBlurTempRTVs.data(),
			Res.Point.VSMBlurTempSRV,
			Res.Point.VSMRTVs.data(),
			Res.Point.Resolution, Res.Point.PageCount, PointBlurRadius);
	}

	// Restore shadow render state
	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::Default);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidFrontCull);
}

// ============================================================
// RenderDirectionalShadows — CSM cascade별 depth 렌더링
// ============================================================

void FShadowMapPass::RenderDirectionalShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	if (!Res.CSM.IsValid()) return;

	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	if (!Env.HasGlobalDirectionalLight()) return;

	constexpr int32 NumCascades = MAX_SHADOW_CASCADES;

	FGlobalDirectionalLightParams DirectionalParams = Env.GetGlobalDirectionalLightParams();

	// b5 Bias/SlopeBias/Sharpen: FShadowSettings override > per-light 값
	const auto& Settings = FShadowSettings::Get();
	ShadowCBCache.ShadowBias       = Settings.GetBias().value_or(DirectionalParams.ShadowBias);
	ShadowCBCache.ShadowSlopeBias  = Settings.GetSlopeBias().value_or(DirectionalParams.ShadowSlopeBias);
	ShadowCBCache.ShadowNormalBias = DirectionalParams.ShadowNormalBias;
	ShadowCBCache.ShadowSharpen    = Settings.GetSharpen().value_or(DirectionalParams.ShadowSharpen);
	ShadowCBCache.CSMBlendRange    = (std::max)(0.0f, Settings.GetEffectiveCSMBlendRange());
	ShadowCBCache.CSMBlendEnabled  = Settings.GetEffectiveCSMBlendEnabled() ? 1u : 0u;

	FMatrix CameraView = Ctx.Frame.View;
	FMatrix CameraProj = Ctx.Frame.Proj;

	const float CameraNearZ = Ctx.Frame.NearClip;
	const float CameraFarZ = Ctx.Frame.FarClip;

	//해당 범위까지 directional light에 대한 shadow가 그려지며, 이 구간을 4개의 cascade로 분할함
	const float ShadowDistance = FShadowSettings::Get().GetEffectiveCSMDistance();
	const float ShadowFarZ = (CameraFarZ < ShadowDistance) ? CameraFarZ : ShadowDistance;
	const float CascadeLambda = FShadowSettings::Get().GetEffectiveCSMCascadeLambda();

	//view frustum을 분할합니다.
	FLightFrustumUtils::FCascadeRange CascadeRanges[NumCascades];
	FLightFrustumUtils::ComputeCascadeRanges(
		CameraNearZ,
		ShadowFarZ,
		NumCascades,
		CascadeLambda,
		CascadeRanges
	);

	ShadowCBCache.CascadeSplits = FVector4(
		CascadeRanges[0].FarZ, CascadeRanges[1].FarZ,
		CascadeRanges[2].FarZ, CascadeRanges[3].FarZ
	);
	Res.CSM.DebugCascadeNear = FVector4(
		CascadeRanges[0].NearZ, CascadeRanges[1].NearZ,
		CascadeRanges[2].NearZ, CascadeRanges[3].NearZ
	);
	Res.CSM.DebugCascadeFar = ShadowCBCache.CascadeSplits;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	for(int32 i = 0; i < NumCascades; ++i)
	{
		const float CascadeNearZ = CascadeRanges[i].NearZ;
		const float CascadeFarZ = CascadeRanges[i].FarZ;
		const bool bBlendEnabled = ShadowCBCache.CSMBlendEnabled != 0 && ShadowCBCache.CSMBlendRange > 0.0f;
		const float RenderNearZ = bBlendEnabled
			? (std::max)(CameraNearZ, CascadeNearZ - ShadowCBCache.CSMBlendRange)
			: CascadeNearZ;
		const float RenderFarZ = bBlendEnabled
			? (std::min)(ShadowFarZ, CascadeFarZ + ShadowCBCache.CSMBlendRange)
			: CascadeFarZ;

		FLightFrustumUtils::FDirectionalLightViewProj DirectionalVP
			= FLightFrustumUtils::BuildDirectionalLightCascadeViewProj(
				DirectionalParams, CameraView, CameraProj,
				CameraNearZ, CameraFarZ,
				RenderNearZ, RenderFarZ);

		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(DirectionalVP.ViewProj);

		UploadLightViewProj(DC, DirectionalVP.ViewProj);

		if (CurrentFilterMode == EShadowFilterMode::VSM && Res.CSM.IsVSMValid())
		{
			float clearColor[4] = {0.f, 0.f, 0.f, 0.f};
			DC->ClearRenderTargetView(Res.CSM.VSMRTV[i], clearColor);
			DC->ClearDepthStencilView(Res.CSM.VSMDSV[i], D3D11_CLEAR_DEPTH, 0.0f, 0);
			DC->OMSetRenderTargets(1, &Res.CSM.VSMRTV[i], Res.CSM.VSMDSV[i]);
		}
		else
		{
			DC->ClearDepthStencilView(Res.CSM.DSV[i], D3D11_CLEAR_DEPTH, 0.0f, 0);
			DC->OMSetRenderTargets(0, nullptr, Res.CSM.DSV[i]);
		}

		D3D11_VIEWPORT ShadowVP = {};
		ShadowVP.Width = static_cast<float>(Res.CSM.Resolution);
		ShadowVP.Height = static_cast<float>(Res.CSM.Resolution);
		ShadowVP.MinDepth = 0.0f;
		ShadowVP.MaxDepth = 1.0f;

		DC->RSSetViewports(1, &ShadowVP);
		DrawShadowCasters(Ctx, LightFrustum);
		SHADOW_STATS_ADD_CASTER(DirectionalLight, LastDrawCasterCount);

		ShadowCBCache.CSMViewProj[i] = DirectionalVP.ViewProj;
	}
	ShadowCBCache.NumCSMCascades = NumCascades;
	SHADOW_STATS_ADD_SHADOW_LIGHT(DirectionalLight);
}

// ============================================================
// RenderSpotShadows — 아틀라스 기반 Spot Shadow 렌더링
// ============================================================

void FShadowMapPass::RenderSpotShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();

	// envIndex → shadowDataIdx 매핑 초기화 (-1 = shadow 없음)
	SpotShadowIndexMap.assign(NumSpots, -1);

	if (NumSpots == 0) return;

	const uint32 ShadowSpotCount = static_cast<uint32>(VisibleShadowSpotIndices.size());
	if (ShadowSpotCount == 0) return;
	if (!Res.Spot.IsValid()) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);
	const uint32 Resolution = static_cast<uint32>(SpotLightAtlas.GetAtlasSize());
	const float AtlasF = static_cast<float>(Resolution);
	const uint32 MaxPages = Res.Spot.PageCount;

	auto& Frame = Ctx.Frame;
	float FOVy = 2.0f * atanf(1.0f / Frame.Proj.M[1][1]);

	// ── CommitBatch per page (페이지 분배는 EnsureResources에서 완료) ──
	SpotAtlasRegion.clear();
	SpotAtlasRegion.resize(ShadowSpotCount);

	for (uint32 Page = 0; Page < MaxPages; ++Page)
	{
		if (Page >= SpotPageGroups.size() || SpotPageGroups[Page].empty()) continue;
		SpotLightAtlas.Reset();

		for (uint32 visIdx : SpotPageGroups[Page])
		{
			const uint32 LightIdx = VisibleShadowSpotIndices[visIdx];
			SpotLightAtlas.AddToBatch(static_cast<float>(SpotScaledResolutions[visIdx]), static_cast<int32>(LightIdx));
		}

		TArray<FAtlasRegion> PageRegions = SpotLightAtlas.CommitBatch();

		for (uint32 r = 0; r < SpotPageGroups[Page].size(); ++r)
		{
			uint32 visIdx = SpotPageGroups[Page][r];
			if (r < PageRegions.size() && PageRegions[r].bValid)
			{
				SpotAtlasRegion[visIdx] = PageRegions[r];
				SpotAtlasRegion[visIdx].PageIdx = Page;
			}
		}
	}

	// ── Clear all pages ──
	for (uint32 Page = 0; Page < MaxPages; ++Page)
	{
		if (bVSM && Res.Spot.IsVSMValid())
		{
			float clearColor[4] = {0.f, 0.f, 0.f, 0.f};
			DC->ClearRenderTargetView(Res.Spot.VSMRTVs[Page], clearColor);
			DC->ClearDepthStencilView(Res.Spot.VSMDSVs[Page], D3D11_CLEAR_DEPTH, 0.0f, 0);
		}
		else
		{
			DC->ClearDepthStencilView(Res.Spot.DSVs[Page], D3D11_CLEAR_DEPTH, 0.0f, 0);
		}
	}

	TArray<FSpotShadowDataGPU> SpotGPUData;
	SpotGPUData.resize(ShadowSpotCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	uint32 ShadowIdx = 0;
	for (uint32 i = 0; i < ShadowSpotCount; ++i)
	{
		const FAtlasRegion& AtlasRegion = SpotAtlasRegion[i];
		if (!AtlasRegion.bValid) continue;

		const uint32 PageIdx = AtlasRegion.PageIdx;
		const uint32 LightIdx = VisibleShadowSpotIndices[i];

		// Shadow Viewport Computation
		ShadowVP.TopLeftX = static_cast<float>(AtlasRegion.X);
		ShadowVP.TopLeftY = static_cast<float>(AtlasRegion.Y);
		ShadowVP.Width    = static_cast<float>(AtlasRegion.Size);
		ShadowVP.Height   = static_cast<float>(AtlasRegion.Size);

		auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Env.GetSpotLight(LightIdx));
		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(VP.ViewProj);

		UploadLightViewProj(DC, VP.ViewProj);

		if (bVSM && Res.Spot.IsVSMValid())
		{
			DC->OMSetRenderTargets(1, &Res.Spot.VSMRTVs[PageIdx], Res.Spot.VSMDSVs[PageIdx]);
		}
		else
		{
			DC->OMSetRenderTargets(0, nullptr, Res.Spot.DSVs[PageIdx]);
		}
		DC->RSSetViewports(1, &ShadowVP);

		DrawShadowCasters(Ctx, LightFrustum);
		SHADOW_STATS_ADD_CASTER(SpotLight, LastDrawCasterCount);

		float Sharpen = Env.GetSpotLight(LightIdx).ShadowSharpen;
		float HalfSize = std::round((1.0f - Sharpen) * 3.0f); // mirrors ComputePCFHalfSize
		float PaddingUV = HalfSize / AtlasF;
		FVector4 AtlasScaleBias = FVector4(static_cast<float>(AtlasRegion.Size) / AtlasF - 2 * PaddingUV,
										   static_cast<float>(AtlasRegion.Size) / AtlasF - 2 * PaddingUV,
										   static_cast<float>(AtlasRegion.X)	/ AtlasF + PaddingUV,
										   static_cast<float>(AtlasRegion.Y)	/ AtlasF + PaddingUV);
		const FSpotLightParams& SpotLight = Env.GetSpotLight(LightIdx);
		const auto& Settings = FShadowSettings::Get();

		SpotGPUData[ShadowIdx].ViewProj = VP.ViewProj;
		SpotGPUData[ShadowIdx].AtlasScaleBias = AtlasScaleBias;
		SpotGPUData[ShadowIdx].PageIndex = PageIdx;
		SpotGPUData[ShadowIdx].ShadowBias       = Settings.GetBias().value_or(SpotLight.ShadowBias);
		SpotGPUData[ShadowIdx].ShadowSharpen    = Settings.GetSharpen().value_or(SpotLight.ShadowSharpen);
		SpotGPUData[ShadowIdx].ShadowSlopeBias  = Settings.GetSlopeBias().value_or(SpotLight.ShadowSlopeBias);
		SpotGPUData[ShadowIdx].ShadowNormalBias = SpotLight.ShadowNormalBias;

		SpotShadowIndexMap[LightIdx] = static_cast<int32>(ShadowIdx);
		++ShadowIdx;
	}

	if (ShadowIdx > 0 && Res.Spot.DataBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (SUCCEEDED(DC->Map(Res.Spot.DataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			memcpy(Mapped.pData, SpotGPUData.data(), sizeof(FSpotShadowDataGPU) * ShadowIdx);
			DC->Unmap(Res.Spot.DataBuffer, 0);
		}
	}

	ShadowCBCache.NumShadowSpotLights = ShadowIdx;
	for (uint32 s = 0; s < ShadowIdx; ++s)
		SHADOW_STATS_ADD_SHADOW_LIGHT(SpotLight);
}

// ============================================================
// RenderPointShadows — Point Light Atlas Shadow 렌더링
// ============================================================

void FShadowMapPass::RenderPointShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	FSceneEnvironment& SceneEnvironment = Ctx.Scene->GetEnvironment();

	// envIndex → shadowDataIdx 매핑 초기화 (-1 = shadow 없음)
	const uint32 NumPoints = SceneEnvironment.GetNumPointLights();
	PointShadowIndexMap.assign(NumPoints, -1);

	const uint32 ShadowedPointLightCount = static_cast<uint32>(VisibleShadowPointIndices.size());
	if (ShadowedPointLightCount == 0)
	{
		ShadowCBCache.NumShadowPointLights = 0;
		return;
	}

	if (!Res.Point.IsValid() || !Res.Point.DataBuffer)
	{
		ShadowCBCache.NumShadowPointLights = 0;
		return;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);
	const uint32 MaxPages = Res.Point.PageCount;
	const float PointAtlasF = static_cast<float>(Res.Point.Resolution);

	auto& Frame = Ctx.Frame;
	float FOVy = 2.0f * atanf(1.0f / Frame.Proj.M[1][1]);

	// ── CommitBatch per page (페이지 분배는 EnsureResources에서 완료) ──
	PointAtlasRegion.clear();
	PointAtlasRegion.resize(ShadowedPointLightCount * 6);

	for (uint32 Page = 0; Page < MaxPages; ++Page)
	{
		if (Page >= PointPageGroups.size() || PointPageGroups[Page].empty()) continue;
		PointLightAtlas.Reset();

		for (uint32 shadowIdx : PointPageGroups[Page])
		{
			const uint32 LightIdx = VisibleShadowPointIndices[shadowIdx];
			const FPointLightParams& PointLight = SceneEnvironment.GetPointLight(LightIdx);

			for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
			{
				FPointLightParams FaceParams = PointLight;
				FaceParams.CubeMapOrientation = static_cast<ECubeMapOrientation>(FaceIndex);
				PointLightAtlas.AddToBatch(FaceParams, static_cast<float>(PointScaledResolutions[shadowIdx]), static_cast<int32>(LightIdx));
			}
		}

		TArray<FAtlasRegion> PageRegions = PointLightAtlas.CommitBatch();

		// Map results back — 6 regions per light, in insertion order
		uint32 RegionIdx = 0;
		for (uint32 shadowIdx : PointPageGroups[Page])
		{
			bool bAllValid = true;
			for (uint32 f = 0; f < 6; ++f)
			{
				if (RegionIdx + f < PageRegions.size() && PageRegions[RegionIdx + f].bValid)
					PointAtlasRegion[shadowIdx * 6 + f] = PageRegions[RegionIdx + f];
				else
					bAllValid = false;
			}

			if (bAllValid)
			{
				for (uint32 f = 0; f < 6; ++f)
					PointAtlasRegion[shadowIdx * 6 + f].PageIdx = Page;
			}
			else
			{
				for (uint32 f = 0; f < 6; ++f)
					PointAtlasRegion[shadowIdx * 6 + f] = {};
			}
			RegionIdx += 6;
		}
	}

	// ── Clear all pages ──
	for (uint32 Page = 0; Page < MaxPages; ++Page)
	{
		if (bVSM && Res.Point.IsVSMValid())
		{
			float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
			DC->ClearRenderTargetView(Res.Point.VSMRTVs[Page], clearColor);
			DC->ClearDepthStencilView(Res.Point.VSMDSVs[Page], D3D11_CLEAR_DEPTH, 0.0f, 0);
		}
		else
		{
			DC->ClearDepthStencilView(Res.Point.DSVs[Page], D3D11_CLEAR_DEPTH, 0.0f, 0);
		}
	}

	TArray<FPointShadowDataGPU> PointLightShadowGPUData;
	PointLightShadowGPUData.resize(ShadowedPointLightCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	constexpr float ShadowNearZ = 0.1f;

	for (uint32 ShadowedLightIndex = 0; ShadowedLightIndex < ShadowedPointLightCount; ++ShadowedLightIndex)
	{
		const uint32 LightIdx = VisibleShadowPointIndices[ShadowedLightIndex];
		const FPointLightParams& PointLight = SceneEnvironment.GetPointLight(LightIdx);
		// PageIdx comes from any valid face's region (all 6 faces share the same page)
		const uint32 PageIdx = PointAtlasRegion[ShadowedLightIndex * 6].PageIdx;

		PointShadowIndexMap[LightIdx] = static_cast<int32>(ShadowedLightIndex);

		FPointShadowDataGPU& ShadowData = PointLightShadowGPUData[ShadowedLightIndex];
		ShadowData.NearZ = ShadowNearZ;
		ShadowData.FarZ  = PointLight.AttenuationRadius;
		ShadowData.PageIndex = PageIdx;

		const auto& Settings = FShadowSettings::Get();
		ShadowData.ShadowBias       = Settings.GetBias().value_or(PointLight.ShadowBias);
		ShadowData.ShadowSharpen    = Settings.GetSharpen().value_or(PointLight.ShadowSharpen);
		ShadowData.ShadowSlopeBias  = Settings.GetSlopeBias().value_or(PointLight.ShadowSlopeBias);
		ShadowData.ShadowNormalBias = PointLight.ShadowNormalBias;

		float Sharpen = SceneEnvironment.GetPointLight(LightIdx).ShadowSharpen;
		float HalfSize = std::round((1.0f - Sharpen) * 3.0f); // mirrors ComputePCFHalfSize
		float PaddingUV = HalfSize / PointAtlasF;

		for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			const uint32 AtlasIdx = ShadowedLightIndex * 6 + FaceIndex;
			const FAtlasRegion& Region = PointAtlasRegion[AtlasIdx];

			const auto FaceVP = FLightFrustumUtils::BuildPointLightFaceViewProj(PointLight, FaceIndex, ShadowNearZ);
			ShadowData.FaceViewProj[FaceIndex] = FaceVP.ViewProj;

			if (!Region.bValid)
			{
				ShadowData.FaceAtlasScaleBias[FaceIndex] = FVector4(0.f, 0.f, 0.f, 0.f);
				continue;
			}

			ShadowData.FaceAtlasScaleBias[FaceIndex] = FVector4(
				static_cast<float>(Region.Size) / PointAtlasF - 2 * PaddingUV,
				static_cast<float>(Region.Size) / PointAtlasF - 2 * PaddingUV,
				static_cast<float>(Region.X)    / PointAtlasF + PaddingUV,
				static_cast<float>(Region.Y)    / PointAtlasF + PaddingUV
			);

			FConvexVolume LightFrustum;
			LightFrustum.UpdateFromMatrix(FaceVP.ViewProj);
			UploadLightViewProj(DC, FaceVP.ViewProj);

			ShadowVP.TopLeftX = static_cast<float>(Region.X);
			ShadowVP.TopLeftY = static_cast<float>(Region.Y);
			ShadowVP.Width    = static_cast<float>(Region.Size);
			ShadowVP.Height   = static_cast<float>(Region.Size);

			if (bVSM && Res.Point.IsVSMValid())
				DC->OMSetRenderTargets(1, &Res.Point.VSMRTVs[PageIdx], Res.Point.VSMDSVs[PageIdx]);
			else
				DC->OMSetRenderTargets(0, nullptr, Res.Point.DSVs[PageIdx]);

			DC->RSSetViewports(1, &ShadowVP);

			DrawShadowCasters(Ctx, LightFrustum);
			SHADOW_STATS_ADD_CASTER(PointLight, LastDrawCasterCount);
		}

	}

	if (ShadowedPointLightCount > 0 && Res.Point.DataBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (SUCCEEDED(DC->Map(Res.Point.DataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			memcpy(Mapped.pData, PointLightShadowGPUData.data(), sizeof(FPointShadowDataGPU) * ShadowedPointLightCount);
			DC->Unmap(Res.Point.DataBuffer, 0);
		}
	}

	ShadowCBCache.NumShadowPointLights = ShadowedPointLightCount;
	for (uint32 p = 0; p < ShadowedPointLightCount; ++p)
		SHADOW_STATS_ADD_SHADOW_LIGHT(PointLight);
}
