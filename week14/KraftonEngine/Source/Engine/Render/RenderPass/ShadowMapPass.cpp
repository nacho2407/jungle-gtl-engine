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
#include "Render/Resource/Buffer.h"
#include "Render/Types/LightFrustumUtils.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/ShadowStats.h"
#include "Editor/Settings/ProjectSettings.h"
#include <d3d11.h>

REGISTER_RENDER_PASS(FShadowMapPass)

// ============================================================
// 생성 / 소멸
// ============================================================

FShadowMapPass::FShadowMapPass()
{
	SpotLightAtlas.Init(4096.f, 64.f);
	PassType = ERenderPass::ShadowMap;
}

FShadowMapPass::~FShadowMapPass()
{
	ShadowPerObjectCB.Release();
}

// ============================================================
// BeginPass — SRV 언바인딩, 리소스 Ensure, 공용 렌더 상태
// ============================================================

bool FShadowMapPass::BeginPass(const FPassContext& Ctx)
{
	if (!FProjectSettings::Get().bShadows)
		return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// 이전 프레임 Shadow SRV 언바인딩 (DSV/RTV와 동일 리소스 → R/W hazard 방지)
	ID3D11ShaderResourceView* nullSRVs[5] = {};
	DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 3, nullSRVs);       // t21~t23
	DC->PSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 2, nullSRVs);    // t24~t25

	// FilterMode 결정
	CurrentFilterMode = FShadowSettings::Get().GetEffectiveFilterMode();

	// 리소스 Ensure (FilterMode에 따라 depth-only / VSM 분기)
	EnsureResources(Ctx);

	// PerObject CB 생성 (한 번만)
	ID3D11Device* Dev = Ctx.Device.GetDevice();
	if (!ShadowPerObjectCB.GetBuffer())
		ShadowPerObjectCB.Create(Dev, sizeof(FPerObjectConstants));

	// 공용 렌더 상태 세팅
	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::Default);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidBackCull);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (CurrentFilterMode == EShadowFilterMode::VSM)
	{
		// VSM: color + depth 기록
		Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	}
	else
	{
		// Hard/PCF: depth-only (PS 없음)
		Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::NoColor);
	}

	// Shadow CB 캐시 초기화
	ShadowCBCache = {};
	ShadowCBCache.ShadowBias      = FShadowSettings::Get().GetEffectiveBias();
	ShadowCBCache.ShadowSlopeBias = FShadowSettings::Get().GetEffectiveSlopeBias();
	ShadowCBCache.ShadowFilterMode = static_cast<uint32>(CurrentFilterMode);
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

	// Shadow SRV 바인딩 (t21~t23)
	if (ShadowRes.CSMSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &ShadowRes.CSMSRV);
	if (ShadowRes.SpotAtlasSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &ShadowRes.SpotAtlasSRV);
	if (ShadowRes.PointCubeSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapPointCube, 1, &ShadowRes.PointCubeSRV);

	// Per-light StructuredBuffer SRV 바인딩 (t24, t25)
	if (ShadowRes.SpotShadowDataSRV)
		DC->PSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 1, &ShadowRes.SpotShadowDataSRV);
	if (ShadowRes.PointShadowDataSRV)
		DC->PSSetShaderResources(ESystemTexSlot::PointShadowDatas, 1, &ShadowRes.PointShadowDataSRV);

	// Shadow CB (b5) 업데이트
	UpdateShadowCB(Ctx);
}

// ============================================================
// EnsureResources — FilterMode 기반 리소스 Ensure
// ============================================================

void FShadowMapPass::EnsureResources(const FPassContext& Ctx)
{
	ID3D11Device* Dev = Ctx.Device.GetDevice();
	FShadowMapResources& Res = Ctx.Resources.ShadowResources;
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();

	const uint32 Resolution = FShadowSettings::Get().GetEffectiveResolution();

	// CSM (Directional) — 항상 Ensure (cascade 수는 상수)
	Res.EnsureCSM(Dev, Resolution);

	// Spot Atlas — shadow-casting spot 수 기반 page 수 결정
	// TODO: 실제 shadow-casting spot 수 카운트 후 page 수 계산
	// Res.EnsureSpotAtlas(Dev, Resolution, PageCount);

	// Point Cube — shadow-casting point 수 기반
	// TODO: 실제 shadow-casting point 수 카운트 후 cube 수 계산
	// Res.EnsurePointCube(Dev, Resolution, CubeCount);
}

// ============================================================
// UpdateShadowCB — Shadow CB (b5) 데이터 조립 + GPU 업로드
// ============================================================

void FShadowMapPass::UpdateShadowCB(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShadowMapResources& Res = Ctx.Resources.ShadowResources;

	ShadowCBCache.CSMResolution = Res.CSMResolution;
	ShadowCBCache.NumShadowSpotLights  = 0; // TODO: Execute에서 채운 값
	ShadowCBCache.NumShadowPointLights = 0; // TODO: Execute에서 채운 값

	Ctx.Resources.ShadowConstantBuffer.Update(DC, &ShadowCBCache, sizeof(FShadowCBData));
	ID3D11Buffer* b5 = Ctx.Resources.ShadowConstantBuffer.GetBuffer();
	DC->PSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
}

// ============================================================
// DrawShadowCasters — 공용 프록시 순회 + depth-only 렌더링
// ============================================================
// 호출 전: DSV(또는 RTV+DSV), Viewport가 이미 바인딩된 상태.
// VSM 모드에서는 moment PS가 바인딩된 상태.

void FShadowMapPass::DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShader* LastShader = nullptr;

	for (FPrimitiveSceneProxy* Proxy : Ctx.Scene->GetAllProxies())
	{
		if (!Proxy || !Proxy->IsVisible()) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull)) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly)) continue;

		// Light frustum culling
		if (!LightFrustum.IntersectAABB(Proxy->GetCachedBounds())) continue;

		FMeshBuffer* Mesh = Proxy->GetMeshBuffer();
		if (!Mesh || !Mesh->IsValid()) continue;

		FShader* ProxyShader = Proxy->GetShader();
		if (!ProxyShader) continue;

		// VS + InputLayout 바인딩
		if (ProxyShader != LastShader)
		{
			ProxyShader->Bind(DC);
			if (CurrentFilterMode != EShadowFilterMode::VSM)
				DC->PSSetShader(nullptr, nullptr, 0);
			// TODO: VSM 모드 시 moment PS 바인딩
			LastShader = ProxyShader;
		}

		// PerObject CB (b1) — Model 행렬 업로드
		ShadowPerObjectCB.Update(DC, &Proxy->GetPerObjectConstants(), sizeof(FPerObjectConstants));
		ID3D11Buffer* b1 = ShadowPerObjectCB.GetBuffer();
		DC->VSSetConstantBuffers(ECBSlot::PerObject, 1, &b1);

		// VB/IB 바인딩
		ID3D11Buffer* VB = Mesh->GetVertexBuffer().GetBuffer();
		uint32 VBStride = Mesh->GetVertexBuffer().GetStride();
		uint32 Offset = 0;
		DC->IASetVertexBuffers(0, 1, &VB, &VBStride, &Offset);

		ID3D11Buffer* IB = Mesh->GetIndexBuffer().GetBuffer();
		if (IB)
			DC->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

		// 섹션별 드로우
		for (const FMeshSectionDraw& Section : Proxy->GetSectionDraws())
		{
			if (Section.IndexCount == 0) continue;
			DC->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
			SHADOW_STATS_ADD_DRAW_CALL();
		}
	}
}

// ============================================================
// RenderDirectionalShadows — CSM cascade별 depth 렌더링
// ============================================================
// 담당: 팀원 A
// Env에서 DirectionalLight 정보를 얻어 cascade별 ViewProj 생성 후
// CSMDSV[i]에 depth-only 렌더링.

void FShadowMapPass::RenderDirectionalShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	if (!Res.IsCSMValid()) return;

	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	if (!Env.HasGlobalDirectionalLight()) return;

	// TODO: 팀원 A 구현
	// 1. Env.GetGlobalDirectionalLightParams()에서 Direction 획득
	// 2. cascade별 ViewProj 계산 (카메라 frustum 분할 기반)
	// 3. for (i = 0..MAX_SHADOW_CASCADES-1):
	//      DC->ClearDepthStencilView(Res.CSMDSV[i], ...)
	//      DC->OMSetRenderTargets(0, nullptr, Res.CSMDSV[i])  // VSM: RTV + DSV
	//      SetViewport(Res.CSMResolution)
	//      DrawShadowCasters(Ctx, cascadeFrustum)
	//      ShadowCBCache.CSMViewProj[i] = cascadeViewProj
	// 4. ShadowCBCache.NumCSMCascades = 실제 사용 cascade 수
	// 5. ShadowCBCache.CascadeSplits = split distances
}

// ============================================================
// RenderSpotShadows — Spot Light Atlas 렌더링
// ============================================================
// 담당: 팀원 B
// Env에서 shadow-casting SpotLight를 순회하며
// Atlas의 할당된 영역에 depth 렌더링.

void FShadowMapPass::RenderSpotShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();
	if (NumSpots == 0) return;

	// TODO: 팀원 B 구현
	// 1. shadow-casting spot lights 카운트 → EnsureSpotAtlas 호출
	// 2. SpotLightAtlas에서 각 라이트별 rect 할당
	// 3. for each shadow spot:
	//      ViewProj = FLightFrustumUtils::BuildSpotLightViewProj(light)
	//      Set viewport to atlas rect on the correct page
	//      DC->OMSetRenderTargets(0, nullptr, Res.SpotAtlasDSVs[page])  // VSM: RTV + DSV
	//      DrawShadowCasters(Ctx, spotFrustum)
	//      SpotShadowDataGPU[i] = { ViewProj, atlasScaleBias, pageIndex }
	// 4. Upload SpotShadowDataGPU → Res.SpotShadowDataBuffer
	// 5. ShadowCBCache.NumShadowSpotLights = count
}

// ============================================================
// RenderPointShadows — Point Light CubeMap 렌더링
// ============================================================
// 담당: 팀원 C
// Env에서 shadow-casting PointLight를 순회하며
// CubeMap 6면에 depth 렌더링.

void FShadowMapPass::RenderPointShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumPoints = Env.GetNumPointLights();
	if (NumPoints == 0) return;

	// TODO: 팀원 C 구현
	// 1. shadow-casting point lights 카운트 → EnsurePointCube 호출
	// 2. for each shadow point:
	//      for face = 0..5:
	//        ViewProj = FLightFrustumUtils::BuildPointLightFaceViewProj(light, face)
	//        DC->OMSetRenderTargets(0, nullptr, Res.PointCubeDSVs[cubeIdx*6+face])
	//        SetViewport(Res.PointCubeResolution)
	//        DrawShadowCasters(Ctx, faceFrustum)
	//        PointShadowDataGPU[i].FaceViewProj[face] = viewProj
	//      PointShadowDataGPU[i].NearZ/FarZ/CubeArrayIndex = ...
	// 3. Upload PointShadowDataGPU → Res.PointShadowDataBuffer
	// 4. ShadowCBCache.NumShadowPointLights = count
}
