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
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
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
	SpotLightAtlas.Init(4096.f, 64.f);
	PointLightAtlas.Init(4096.f, 64.f);
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

	SCOPE_STAT_CAT("ShadowMapPass", "4_ExecutePass");
	GPU_SCOPE_STAT("ShadowMapPass");
	SHADOW_STATS_RESET();

	FShadowMapResources& ShadowRes = Ctx.Resources.ShadowResources;

	RenderDirectionalShadows(Ctx, ShadowRes);
	RenderSpotShadows(Ctx, ShadowRes);
	RenderPointShadows(Ctx, ShadowRes);
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

	// ── Shadow Stats: 해상도 + 메모리 ──
	SHADOW_STATS_SET_RESOLUTION(ShadowRes.CSM.Resolution);

	// Shadow map 텍스처 메모리 추정 (depth = 4B/pixel, VSM moment = 8B/pixel)
	{
		const uint32 BPP = (CurrentFilterMode == EShadowFilterMode::VSM) ? 4 + 8 : 4; // depth + (moment if VSM)
		uint64 TotalBytes = 0;

		// CSM: Resolution^2 * cascades
		if (ShadowRes.CSM.Resolution > 0)
			TotalBytes += static_cast<uint64>(ShadowRes.CSM.Resolution) * ShadowRes.CSM.Resolution * MAX_SHADOW_CASCADES * BPP;

		// Spot Atlas: AtlasResolution^2 * pages
		if (ShadowRes.Spot.PageCount > 0)
			TotalBytes += static_cast<uint64>(ShadowRes.Spot.Resolution) * ShadowRes.Spot.Resolution * ShadowRes.Spot.PageCount * BPP;

		// Point Atlas: AtlasSize^2
		if (ShadowRes.Point.Resolution > 0)
			TotalBytes += static_cast<uint64>(ShadowRes.Point.Resolution) * ShadowRes.Point.Resolution * BPP;

		SHADOW_STATS_SET_MEMORY(TotalBytes);
	}
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
	uint32 Resolution = FShadowSettings::Get().GetEffectiveCSMResolution();
	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);

	// ── CSM (Directional) — Directional Light가 있을 때만 생성, 없으면 해제 ──
	if (Env.HasGlobalDirectionalLight())
	{
		const FGlobalDirectionalLightParams& DirectionalParams = Env.GetGlobalDirectionalLightParams();
		const float ScaledResolution = static_cast<float>(Resolution) * DirectionalParams.ShadowResolutionScale;
		Resolution = static_cast<uint32>((std::max)(64.0f, (std::min)(ScaledResolution, 8192.0f)));
		
		Res.EnsureCSM(Dev, Resolution);
		if (bVSM) Res.EnsureCSM_VSM(Dev, Resolution);
	}
	else if (Res.CSM.IsValid())
	{
		Res.CSM.Release();
	}

	// ── Spot Atlas — 카메라 프러스텀 컬링 후 가시 라이트만 ──
	const FConvexVolume& CameraFrustum = Ctx.Frame.FrustumVolume;
	VisibleShadowSpotIndices.clear();
	const uint32 NumSpots = Env.GetNumSpotLights();
	for (uint32 i = 0; i < NumSpots; ++i)
	{
		const auto& Light = Env.GetSpotLight(i);
		if (!Light.bCastShadows) continue;
		if (!CameraFrustum.IntersectSphere(Light.Position, Light.AttenuationRadius)) continue;
		VisibleShadowSpotIndices.push_back(i);
		if (VisibleShadowSpotIndices.size() >= MAX_SHADOW_SPOT_LIGHTS) break;
	}
	uint32 ShadowSpotCount = static_cast<uint32>(VisibleShadowSpotIndices.size());

	if (ShadowSpotCount > 0)
	{
		const uint32 SpotRes = static_cast<uint32>(SpotLightAtlas.GetAtlasSize());
		Res.EnsureSpotAtlas(Dev, SpotRes, ShadowSpotCount);
		if (bVSM) Res.EnsureSpotAtlas_VSM(Dev, SpotRes, 1);		// Change 1 to PageCount for multiple atlas pages
	}
	else if (Res.Spot.IsValid())
	{
		Res.Spot.Release();
	}

	// ── Point Atlas — 카메라 프러스텀 컬링 후 가시 라이트만 ──
	// Point Light는 전방향 조명이므로 라이트가 프러스텀 밖에 있어도
	// 그림자가 프러스텀 안의 오브젝트에 드리워질 수 있다.
	// 감쇠 반경의 2배로 컬링 마진을 확장하여 그림자 누락을 방지한다.
	VisibleShadowPointIndices.clear();
	const uint32 NumPoints = Env.GetNumPointLights();
	for (uint32 i = 0; i < NumPoints; ++i)
	{
		const auto& Light = Env.GetPointLight(i);
		if (!Light.bCastShadows) continue;
		if (!CameraFrustum.IntersectSphere(Light.Position, Light.AttenuationRadius * 2.0f)) continue;
		VisibleShadowPointIndices.push_back(i);
		if (VisibleShadowPointIndices.size() >= MAX_SHADOW_POINT_LIGHTS) break;
	}
	uint32 ShadowPointCount = static_cast<uint32>(VisibleShadowPointIndices.size());

	if (ShadowPointCount > 0)
	{
		const uint32 PointAtlasSize = static_cast<uint32>(PointLightAtlas.GetAtlasSize());
		Res.EnsurePointAtlas(Dev, PointAtlasSize, ShadowPointCount);
		if (bVSM) Res.EnsurePointAtlas_VSM(Dev, PointAtlasSize);
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

	Ctx.Resources.ShadowConstantBuffer.Update(DC, &ShadowCBCache, sizeof(FShadowCBData));
	ID3D11Buffer* b5 = Ctx.Resources.ShadowConstantBuffer.GetBuffer();
	DC->PSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
	DC->VSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
}

// ============================================================
// DrawShadowCasters — 공용 프록시 순회 + depth-only 렌더링
// ============================================================

void FShadowMapPass::DrawShadowCasters(ID3D11DeviceContext* DC, FScene& Scene, const FConvexVolume& LightFrustum, FSpatialPartition* Partition)
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
	for (FPrimitiveSceneProxy* Proxy : *ProxyList)
	{
		if (!Proxy || !Proxy->IsVisible()) continue;
		if (!Proxy->CastsShadow()) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull)) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly)) continue;

		if (!Partition && !LightFrustum.IntersectAABB(Proxy->GetCachedBounds())) continue;

		FMeshBuffer* Mesh = Proxy->GetMeshBuffer();
		if (!Mesh || !Mesh->IsValid()) continue;

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
}

void FShadowMapPass::DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum)
{
	FSpatialPartition* Partition = nullptr;
	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorld())
			Partition = &World->GetPartition();
	}
	DrawShadowCasters(Ctx.Device.GetDeviceContext(), *Ctx.Scene, LightFrustum, Partition);
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
	SpotLightAtlas.Reset();
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();
	if (NumSpots == 0) return;

	const uint32 ShadowSpotCount = static_cast<uint32>(VisibleShadowSpotIndices.size());
	if (ShadowSpotCount == 0) return;
	if (!Res.Spot.IsValid()) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	const bool bVSM = (CurrentFilterMode == EShadowFilterMode::VSM);
	const uint32 Resolution = static_cast<uint32>(SpotLightAtlas.GetAtlasSize());

	if (bVSM && Res.Spot.IsVSMValid())
	{
		float clearColor[4] = {0.f, 0.f, 0.f, 0.f};
		DC->ClearRenderTargetView(Res.Spot.VSMRTVs[0], clearColor);
		DC->ClearDepthStencilView(Res.Spot.VSMDSVs[0], D3D11_CLEAR_DEPTH, 0.0f, 0);
	}
	else
	{
		DC->ClearDepthStencilView(Res.Spot.DSVs[0], D3D11_CLEAR_DEPTH, 0.0f, 0);
	}

	TArray<FSpotShadowDataGPU> SpotGPUData;
	SpotGPUData.resize(ShadowSpotCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	uint32 ShadowIdx = 0;
	auto& Frame = Ctx.Frame;
	float FOVy = 2.0f * atanf(1.0f / Frame.Proj.M[1][1]);

	for (uint32 idx : VisibleShadowSpotIndices) {
		const FSpotLightParams& Light = Env.GetSpotLight(idx);
		SpotLightAtlas.AddToBatch(Light, Frame.CameraPosition, Frame.CameraForward, FOVy, Frame.ViewportHeight, static_cast<int32>(idx));
	}
	SpotAtlasRegion = SpotLightAtlas.CommitBatch();

	for (uint32 i = 0; i < SpotAtlasRegion.size(); ++i)
	{
		if (ShadowIdx >= ShadowSpotCount) break;
		FAtlasRegion AtlasRegion = SpotAtlasRegion[i];
		if (!AtlasRegion.bValid) continue;

		// Shadow Viewport Computation
		ShadowVP.TopLeftX = static_cast<float>(AtlasRegion.X);
		ShadowVP.TopLeftY = static_cast<float>(AtlasRegion.Y);
		ShadowVP.Width    = static_cast<float>(AtlasRegion.Size);
		ShadowVP.Height   = static_cast<float>(AtlasRegion.Size);

		const uint32 LightIdx = VisibleShadowSpotIndices[i];
		auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Env.GetSpotLight(LightIdx));
		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(VP.ViewProj);

		UploadLightViewProj(DC, VP.ViewProj);

		if (bVSM && Res.Spot.IsVSMValid())
		{
			DC->OMSetRenderTargets(1, &Res.Spot.VSMRTVs[0], Res.Spot.VSMDSVs[0]);
		}
		else
		{
			DC->OMSetRenderTargets(0, nullptr, Res.Spot.DSVs[0]);
		}
		DC->RSSetViewports(1, &ShadowVP);

		DrawShadowCasters(Ctx, LightFrustum);
		SHADOW_STATS_ADD_CASTER(SpotLight, LastDrawCasterCount);

		float AtlasF = static_cast<float>(Resolution);
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
		SpotGPUData[ShadowIdx].PageIndex = 0;
		SpotGPUData[ShadowIdx].ShadowBias       = Settings.GetBias().value_or(SpotLight.ShadowBias);
		SpotGPUData[ShadowIdx].ShadowSharpen    = Settings.GetSharpen().value_or(SpotLight.ShadowSharpen);
		SpotGPUData[ShadowIdx].ShadowSlopeBias  = Settings.GetSlopeBias().value_or(SpotLight.ShadowSlopeBias);
		SpotGPUData[ShadowIdx].ShadowNormalBias = SpotLight.ShadowNormalBias;

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
	PointLightAtlas.Reset();

	FSceneEnvironment& SceneEnvironment = Ctx.Scene->GetEnvironment();

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

	// 아틀라스 배치: 라이트당 6개 엔트리 (face별 하나씩)
	auto& Frame = Ctx.Frame;
	float FOVy = 2.0f * atanf(1.0f / Frame.Proj.M[1][1]);

	for (uint32 idx : VisibleShadowPointIndices)
	{
		const FPointLightParams& PointLight = SceneEnvironment.GetPointLight(idx);

		for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			FPointLightParams FaceParams = PointLight;
			FaceParams.CubeMapOrientation = static_cast<ECubeMapOrientation>(FaceIndex);
			PointLightAtlas.AddToBatch(FaceParams, Frame.CameraPosition, Frame.CameraForward, FOVy, Frame.ViewportHeight, static_cast<int32>(idx));
		}
	}

	PointAtlasRegion = PointLightAtlas.CommitBatch();

	// 아틀라스 전체를 한 번만 클리어
	if (bVSM && Res.Point.IsVSMValid())
	{
		float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
		DC->ClearRenderTargetView(Res.Point.VSMRTV, clearColor);
		DC->ClearDepthStencilView(Res.Point.VSMDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
	}
	else
	{
		DC->ClearDepthStencilView(Res.Point.DSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
	}

	TArray<FPointShadowDataGPU> PointLightShadowGPUData;
	PointLightShadowGPUData.resize(ShadowedPointLightCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	constexpr float ShadowNearZ = 0.1f;
	const float AtlasF = static_cast<float>(Res.Point.Resolution);

	for (uint32 ShadowedLightIndex = 0; ShadowedLightIndex < ShadowedPointLightCount; ++ShadowedLightIndex)
	{
		const uint32 LightIdx = VisibleShadowPointIndices[ShadowedLightIndex];
		const FPointLightParams& PointLight = SceneEnvironment.GetPointLight(LightIdx);

		FPointShadowDataGPU& ShadowData = PointLightShadowGPUData[ShadowedLightIndex];
		ShadowData.NearZ = ShadowNearZ;
		ShadowData.FarZ  = PointLight.AttenuationRadius;

		const auto& Settings = FShadowSettings::Get();
		ShadowData.ShadowBias       = Settings.GetBias().value_or(PointLight.ShadowBias);
		ShadowData.ShadowSharpen    = Settings.GetSharpen().value_or(PointLight.ShadowSharpen);
		ShadowData.ShadowSlopeBias  = Settings.GetSlopeBias().value_or(PointLight.ShadowSlopeBias);
		ShadowData.ShadowNormalBias = PointLight.ShadowNormalBias;

		float Sharpen = SceneEnvironment.GetPointLight(LightIdx).ShadowSharpen;
		float HalfSize = std::round((1.0f - Sharpen) * 3.0f); // mirrors ComputePCFHalfSize
		float PaddingUV = HalfSize / AtlasF;

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
				static_cast<float>(Region.Size) / AtlasF - 2 * PaddingUV,
				static_cast<float>(Region.Size) / AtlasF - 2 * PaddingUV,
				static_cast<float>(Region.X)    / AtlasF + PaddingUV,
				static_cast<float>(Region.Y)    / AtlasF + PaddingUV
			);

			FConvexVolume LightFrustum;
			LightFrustum.UpdateFromMatrix(FaceVP.ViewProj);
			UploadLightViewProj(DC, FaceVP.ViewProj);

			ShadowVP.TopLeftX = static_cast<float>(Region.X);
			ShadowVP.TopLeftY = static_cast<float>(Region.Y);
			ShadowVP.Width    = static_cast<float>(Region.Size);
			ShadowVP.Height   = static_cast<float>(Region.Size);

			if (bVSM && Res.Point.IsVSMValid())
				DC->OMSetRenderTargets(1, &Res.Point.VSMRTV, Res.Point.VSMDSV);
			else
				DC->OMSetRenderTargets(0, nullptr, Res.Point.DSV);

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
