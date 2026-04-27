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
#include "Editor/Settings/ProjectSettings.h"
#include "Collision/SpatialPartition.h"
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
	ShadowLightCB.Release();
}

// ============================================================
// SetupShadowRenderState — SRV 언바인딩 + 공용 렌더 상태 (PSM / Global 공용)
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
	Resources.SetRasterizerState(Device, ERasterizerState::SolidBackCull);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (CurrentFilterMode == EShadowFilterMode::VSM)
		Resources.SetBlendState(Device, EBlendState::Opaque);
	else
		Resources.SetBlendState(Device, EBlendState::NoColor);

	// Shadow CB 캐시 초기화
	ShadowCBCache = {};
	ShadowCBCache.ShadowBias      = FShadowSettings::Get().GetEffectiveBias();
	ShadowCBCache.ShadowSlopeBias = FShadowSettings::Get().GetEffectiveSlopeBias();
	ShadowCBCache.ShadowFilterMode = static_cast<uint32>(CurrentFilterMode);
}

// ============================================================
// BeginPass — PSM 전용 (per-viewport Directional)
// ============================================================

bool FShadowMapPass::BeginPass(const FPassContext& Ctx)
{
	const auto& Shadow = FProjectSettings::Get().Shadow;
	if (!Shadow.bEnabled || !Shadow.bPSM)
		return false;

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

	// PSM 모드: Directional만 처리 (Spot/Point는 Global에서 처리)
	RenderDirectionalShadows(Ctx, ShadowRes);

	RenderSpotShadows(Ctx, ShadowRes);
	RenderPointShadows(Ctx, ShadowRes);
}

// ============================================================
// EndPass — 메인 RT 복원, Shadow SRV 바인딩, Shadow CB 업데이트
// ============================================================

// ============================================================
// BindShadowSRVs — Shadow SRV 바인딩 (PSM / Global 공용)
// ============================================================

void FShadowMapPass::BindShadowSRVs(ID3D11DeviceContext* DC, FShadowMapResources& Res)
{
	if (Res.CSMSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 1, &Res.CSMSRV);
	if (Res.SpotAtlasSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapSpotAtlas, 1, &Res.SpotAtlasSRV);
	if (Res.PointCubeSRV)
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMapPointCube, 1, &Res.PointCubeSRV);

	if (Res.SpotShadowDataSRV)
		DC->PSSetShaderResources(ESystemTexSlot::SpotShadowDatas, 1, &Res.SpotShadowDataSRV);
	if (Res.PointShadowDataSRV)
		DC->PSSetShaderResources(ESystemTexSlot::PointShadowDatas, 1, &Res.PointShadowDataSRV);
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

	BindShadowSRVs(DC, ShadowRes);
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

void FShadowMapPass::UpdateShadowCB(ID3D11DeviceContext* DC, FSystemResources& Resources, FShadowMapResources& Res)
{
	ShadowCBCache.CSMResolution = Res.CSMResolution;

	Resources.ShadowConstantBuffer.Update(DC, &ShadowCBCache, sizeof(FShadowCBData));
	ID3D11Buffer* b5 = Resources.ShadowConstantBuffer.GetBuffer();
	DC->PSSetConstantBuffers(ECBSlot::Shadow, 1, &b5);
}

void FShadowMapPass::UpdateShadowCB(const FPassContext& Ctx)
{
	UpdateShadowCB(Ctx.Device.GetDeviceContext(), Ctx.Resources, Ctx.Resources.ShadowResources);
}

// ============================================================
// DrawShadowCasters — 공용 프록시 순회 + depth-only 렌더링
// ============================================================
// 호출 전: DSV(또는 RTV+DSV), Viewport가 이미 바인딩된 상태.
// VSM 모드에서는 moment PS가 바인딩된 상태.

void FShadowMapPass::DrawShadowCasters(ID3D11DeviceContext* DC, FScene& Scene, const FConvexVolume& LightFrustum, FSpatialPartition* Partition)
{
	// ShadowDepth 전용 셰이더 바인딩 (VS + InputLayout + PS)
	FShader* ShadowShader = FShaderManager::Get().GetOrCreate(EShaderPath::ShadowDepth);
	if (!ShadowShader || !ShadowShader->IsValid()) return;

	ShadowShader->Bind(DC);

	// Hard/PCF: depth-only (PS 불필요)
	if (CurrentFilterMode != EShadowFilterMode::VSM)
		DC->PSSetShader(nullptr, nullptr, 0);

	// Octree broad-phase → 후보 프록시만 순회
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

	for (FPrimitiveSceneProxy* Proxy : *ProxyList)
	{
		if (!Proxy || !Proxy->IsVisible()) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull)) continue;
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly)) continue;

		// Octree가 없으면 per-proxy frustum culling fallback
		if (!Partition && !LightFrustum.IntersectAABB(Proxy->GetCachedBounds())) continue;

		FMeshBuffer* Mesh = Proxy->GetMeshBuffer();
		if (!Mesh || !Mesh->IsValid()) continue;

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

void FShadowMapPass::DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum)
{
	DrawShadowCasters(Ctx.Device.GetDeviceContext(), *Ctx.Scene, LightFrustum);
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

	constexpr int32 NumCascades = MAX_SHADOW_CASCADES;

	FGlobalDirectionalLightParams DirectionalParams = Env.GetGlobalDirectionalLightParams();

	FMatrix CameraView = Ctx.Frame.View;
	FMatrix CameraProj = Ctx.Frame.Proj;

	const float CameraNearZ = Ctx.Frame.NearClip;
	const float CameraFarZ = Ctx.Frame.FarClip;

	//CSM에서 실제로 shadow를 생성하는 최대 길이.
	//FarClip 전체를 쓰면 C0도 지나치게 넓어져 근거리 품질이 낮아짐.
	const float ShadowDistance = (CameraFarZ < 300.0f) ? CameraFarZ : 300.0f;
	const float ShadowFarZ = (CameraFarZ < ShadowDistance) ? CameraFarZ : ShadowDistance;

	FLightFrustumUtils::FCascadeRange CascadeRanges[NumCascades];
	FLightFrustumUtils::ComputeCascadeRanges(
		CameraNearZ,
		ShadowFarZ,
		NumCascades,
		0.85f,
		CascadeRanges
	);

	ShadowCBCache.CascadeSplits = FVector4(
		CascadeRanges[0].FarZ,
		CascadeRanges[1].FarZ,
		CascadeRanges[2].FarZ,
		CascadeRanges[3].FarZ
	);
	//ImGui 디버그용
	Res.CSMDebugCascadeNear = FVector4(
		CascadeRanges[0].NearZ,
		CascadeRanges[1].NearZ,
		CascadeRanges[2].NearZ,
		CascadeRanges[3].NearZ
	);
	Res.CSMDebugCascadeFar = ShadowCBCache.CascadeSplits;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	for(int32 i = 0; i < NumCascades; ++i)
	{
		const float CascadeNearZ = CascadeRanges[i].NearZ;
		const float CascadeFarZ = CascadeRanges[i].FarZ;

		//그걸로 빛 기준 view proj 생성
		FLightFrustumUtils::FDirectionalLightViewProj DirectionalVP
			= FLightFrustumUtils::BuildDirectionalLightCascadeViewProj(
				DirectionalParams, CameraView, CameraProj,
				CameraNearZ,CameraFarZ,
				CascadeNearZ, CascadeFarZ);
		
		//frustum 생성
		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(DirectionalVP.ViewProj);

		UploadLightViewProj(DC, DirectionalVP.ViewProj);

		//reverse-Z
		DC->ClearDepthStencilView(Res.CSMDSV[i], D3D11_CLEAR_DEPTH, 0.0f, 0);
		DC->OMSetRenderTargets(0, nullptr, Res.CSMDSV[i]);

		D3D11_VIEWPORT ShadowVP = {};
		ShadowVP.TopLeftX = 0.0f;
		ShadowVP.TopLeftY = 0.0f;
		ShadowVP.Width = static_cast<float>(Res.CSMResolution);
		ShadowVP.Height = static_cast<float>(Res.CSMResolution);
		ShadowVP.MinDepth = 0.0f;
		ShadowVP.MaxDepth = 1.0f;
		
		DC->RSSetViewports(1, &ShadowVP);

		DrawShadowCasters(Ctx, LightFrustum);

		ShadowCBCache.CSMViewProj[i] = DirectionalVP.ViewProj;
	}
	ShadowCBCache.NumCSMCascades = NumCascades;
}

// ============================================================
// RenderSpotShadows — 1 texture per spotlight (Atlas 우회 테스트용)
// ============================================================
// Texture2DArray의 slice를 1 spot = 1 slice로 사용.
// Atlas quadtree 완성 후 교체 예정.

void FShadowMapPass::RenderSpotShadows(ID3D11DeviceContext* DC, FD3DDevice& Device, FSystemResources& Resources, FScene& Scene, FShadowMapResources& Res, FSpatialPartition* Partition)
{
	SpotLightAtlas.Reset();
	const FSceneEnvironment& Env = Scene.GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();
	if (NumSpots == 0) return;

	// shadow-casting spot 수 카운트
	uint32 ShadowSpotCount = 0;
	for (uint32 i = 0; i < NumSpots; ++i)
	{
		if (Env.GetSpotLight(i).bCastShadows)
			++ShadowSpotCount;
	}
	if (ShadowSpotCount == 0) return;

	ShadowSpotCount = (ShadowSpotCount > MAX_SHADOW_SPOT_LIGHTS)
		? MAX_SHADOW_SPOT_LIGHTS : ShadowSpotCount;

	// 1 spot = 1 slice → SpotAtlas를 slice 배열로 사용
	//const uint32 Resolution = FShadowSettings::Get().GetEffectiveResolution();
	const uint32 Resolution = (uint32)SpotLightAtlas.GetAtlasSize();
	Res.EnsureSpotAtlas(Device.GetDevice(), Resolution, ShadowSpotCount);
	if (!Res.IsSpotValid()) return;

	// per-light GPU 데이터 준비
	TArray<FSpotShadowDataGPU> SpotGPUData;
	SpotGPUData.resize(ShadowSpotCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.Width    = static_cast<float>(Resolution);
	ShadowVP.Height   = static_cast<float>(Resolution);
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	uint32 ShadowIdx = 0;
	for (uint32 i = 0; i < NumSpots && ShadowIdx < ShadowSpotCount; ++i)
	{
		const FSpotLightParams& Light = Env.GetSpotLight(i);
		if (!Light.bCastShadows) continue;

		// ViewProj 계산
		auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Light);
		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(VP.ViewProj);

		// b2에 LightViewProj 업로드
		UploadLightViewProj(DC, VP.ViewProj);

		// DSV 바인딩 (slice = ShadowIdx)
		DC->ClearDepthStencilView(Res.SpotAtlasDSVs[ShadowIdx], D3D11_CLEAR_DEPTH, 0.0f, 0);
		DC->OMSetRenderTargets(0, nullptr, Res.SpotAtlasDSVs[ShadowIdx]);
		DC->RSSetViewports(1, &ShadowVP);

		// depth 렌더링
		DrawShadowCasters(DC, Scene, LightFrustum, Partition);

		// GPU 데이터 기록 — 1:1 slice, UV = 전체 (atlas 안 쓰므로)
		SpotGPUData[ShadowIdx].ViewProj        = VP.ViewProj;
		SpotGPUData[ShadowIdx].AtlasScaleBias  = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
		SpotGPUData[ShadowIdx].PageIndex        = ShadowIdx;

		++ShadowIdx;
	}

	// StructuredBuffer 업로드
	if (ShadowIdx > 0 && Res.SpotShadowDataBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (SUCCEEDED(DC->Map(Res.SpotShadowDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			memcpy(Mapped.pData, SpotGPUData.data(), sizeof(FSpotShadowDataGPU) * ShadowIdx);
			DC->Unmap(Res.SpotShadowDataBuffer, 0);
		}
	}

	ShadowCBCache.NumShadowSpotLights = ShadowIdx;
}

void FShadowMapPass::RenderSpotShadows(const FPassContext& Ctx, FShadowMapResources& Res) {
	SpotLightAtlas.Reset();
	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();
	if (NumSpots == 0) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// shadow-casting spot 수 카운트
	uint32 ShadowSpotCount = 0;
	for (uint32 i = 0; i < NumSpots; ++i)
	{
		if (Env.GetSpotLight(i).bCastShadows)
			++ShadowSpotCount;
	}
	if (ShadowSpotCount == 0) return;

	ShadowSpotCount = (ShadowSpotCount > MAX_SHADOW_SPOT_LIGHTS)
		? MAX_SHADOW_SPOT_LIGHTS : ShadowSpotCount;

	// 1 spot = 1 slice → SpotAtlas를 slice 배열로 사용
	const uint32 Resolution = (uint32)SpotLightAtlas.GetAtlasSize();
	Res.EnsureSpotAtlas(Ctx.Device.GetDevice(), Resolution, ShadowSpotCount);
	if (!Res.IsSpotValid()) return;
	DC->ClearDepthStencilView(Res.SpotAtlasDSVs[0], D3D11_CLEAR_DEPTH, 0.0f, 0);

	// per-light GPU 데이터 준비
	TArray<FSpotShadowDataGPU> SpotGPUData;
	SpotGPUData.resize(ShadowSpotCount);

	D3D11_VIEWPORT ShadowVP = {};
	ShadowVP.MinDepth = 0.0f;
	ShadowVP.MaxDepth = 1.0f;

	uint32 ShadowIdx = 0;
	auto& Frame = Ctx.Frame;
	float FOVy = 2.0f * atanf(1.0f / Frame.Proj.M[1][1]);

	for (uint32 i = 0; i < NumSpots; ++i) {
		const FSpotLightParams& Light = Env.GetSpotLight(i);
		SpotLightAtlas.AddToBatch(Light.ToLightInfo(), Frame.CameraPosition, Frame.CameraForward, FOVy, Frame.ViewportHeight);
	}
	TArray<FAtlasRegion> AtlasRegions = SpotLightAtlas.CommitBatch();

	for (uint32 i = 0; i < AtlasRegions.size(); ++i)
	{
		if (ShadowIdx >= ShadowSpotCount) break;
		FAtlasRegion AtlasRegion = AtlasRegions[i];
		if (!AtlasRegion.bValid) continue;

		// Shadow Viewport COmputation
		ShadowVP.TopLeftX = static_cast<float>(AtlasRegion.X);
		ShadowVP.TopLeftY = static_cast<float>(AtlasRegion.Y);
		ShadowVP.Width    = static_cast<float>(AtlasRegion.Size);
		ShadowVP.Height   = static_cast<float>(AtlasRegion.Size);

		// ViewProj 계산
		auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Env.GetSpotLight(i));
		FConvexVolume LightFrustum;
		LightFrustum.UpdateFromMatrix(VP.ViewProj);

		// b2에 LightViewProj 업로드
		UploadLightViewProj(DC, VP.ViewProj);

		// DSV 바인딩 (slice = ShadowIdx)
		DC->OMSetRenderTargets(0, nullptr, Res.SpotAtlasDSVs[0]);
		DC->RSSetViewports(1, &ShadowVP);

		// depth 렌더링
		DrawShadowCasters(Ctx, LightFrustum);

		// GPU 데이터 기록 — 1:1 slice, UV = 전체 (atlas 안 쓰므로)
		float AtlasF = static_cast<float>(Resolution);
		FVector4 AtlasScaleBias = FVector4(static_cast<float>(AtlasRegion.Size) / AtlasF,
										   static_cast<float>(AtlasRegion.Size) / AtlasF,
										   static_cast<float>(AtlasRegion.X)	/ AtlasF,
										   static_cast<float>(AtlasRegion.Y)	/ AtlasF);
		SpotGPUData[ShadowIdx].ViewProj = VP.ViewProj;
		SpotGPUData[ShadowIdx].AtlasScaleBias = AtlasScaleBias;
		SpotGPUData[ShadowIdx].PageIndex = 0;

		++ShadowIdx;
	}

	// StructuredBuffer 업로드
	if (ShadowIdx > 0 && Res.SpotShadowDataBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (SUCCEEDED(DC->Map(Res.SpotShadowDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			memcpy(Mapped.pData, SpotGPUData.data(), sizeof(FSpotShadowDataGPU) * ShadowIdx);
			DC->Unmap(Res.SpotShadowDataBuffer, 0);
		}
	}

	ShadowCBCache.NumShadowSpotLights = ShadowIdx;
}

void FShadowMapPass::RenderPointShadows(const FPassContext& Ctx, FShadowMapResources& Res)
{
	FSceneEnvironment& SceneEnvironment = Ctx.Scene->GetEnvironment();
	const uint32 NumPointLights = SceneEnvironment.GetNumPointLights();

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	uint32 ShadowPointLightCount = 0;
	for (uint32 PointLightIndex = 0; PointLightIndex < NumPointLights; ++PointLightIndex)
	{
		if (SceneEnvironment.GetPointLight(PointLightIndex).bCastShadows)
		{
			++ShadowPointLightCount;
		}
	}

	if (ShadowPointLightCount == 0)
	{
		ShadowCBCache.NumShadowPointLights = 0;
		return;
	}

	// Clamped to MAX_SHADOW_POINT_LIGHTS
	if (ShadowPointLightCount > MAX_SHADOW_POINT_LIGHTS)
	{
		ShadowPointLightCount = MAX_SHADOW_POINT_LIGHTS;
	}

	const uint32 ShadowResolution = FShadowSettings::Get().GetEffectiveResolution();
	Res.EnsurePointCube(Ctx.Device.GetDevice(), ShadowResolution, ShadowPointLightCount);
	if (!Res.IsPointValid() || !Res.PointCubeDSVs || !Res.PointShadowDataBuffer)
	{
		ShadowCBCache.NumShadowPointLights = 0;
		return;
	}

	TArray<FPointShadowDataGPU> PointLightShadowGPUData;
	PointLightShadowGPUData.resize(ShadowPointLightCount);

	D3D11_VIEWPORT ShadowViewport = {};
	ShadowViewport.Width    = static_cast<float>(ShadowResolution);
	ShadowViewport.Height   = static_cast<float>(ShadowResolution);
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;

	constexpr float ShadowNearZ = 0.1f;
	uint32 ShadowPointLightIndex = 0;
	for (uint32 PointLightIndex = 0; PointLightIndex < NumPointLights && ShadowPointLightIndex < ShadowPointLightCount; ++PointLightIndex)
	{
		const FPointLightParams &Light = SceneEnvironment.GetPointLight(PointLightIndex);
		if (!Light.bCastShadows)
		{
			continue;
		}

		FPointShadowDataGPU &ShadowData = PointLightShadowGPUData[ShadowPointLightIndex];
		ShadowData.NearZ = ShadowNearZ;
		ShadowData.FarZ = Light.AttenuationRadius;
		ShadowData.CubeArrayIndex = ShadowPointLightIndex;

		for (uint32 CubeFaceIndex = 0; CubeFaceIndex < 6; ++CubeFaceIndex)
		{
			const auto FaceViewProjection = FLightFrustumUtils::BuildPointLightFaceViewProj(Light, CubeFaceIndex, ShadowNearZ);
			ShadowData.FaceViewProj[CubeFaceIndex] = FaceViewProjection.ViewProj;

			FConvexVolume LightFrustum;
			LightFrustum.UpdateFromMatrix(FaceViewProjection.ViewProj);

			UploadLightViewProj(DC, FaceViewProjection.ViewProj);

			uint32 DSVFaceIndex = ShadowPointLightIndex * 6 + CubeFaceIndex;
			ID3D11DepthStencilView *FaceDepthStencilView = Res.PointCubeDSVs[DSVFaceIndex];
			DC->ClearDepthStencilView(FaceDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0);
			DC->OMSetRenderTargets(0, nullptr, FaceDepthStencilView);
			DC->RSSetViewports(1, &ShadowViewport);

			DrawShadowCasters(Ctx, LightFrustum);
		}

		++ShadowPointLightIndex;
	}

	if (ShadowPointLightIndex > 0 && Res.PointShadowDataBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE MappedPointShadowData = {};
		if (SUCCEEDED(DC->Map(Res.PointShadowDataBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedPointShadowData)))
		{
			memcpy(MappedPointShadowData.pData, PointLightShadowGPUData.data(), sizeof(FPointShadowDataGPU) * ShadowPointLightIndex);
			DC->Unmap(Res.PointShadowDataBuffer, 0);
		}
	}

	ShadowCBCache.NumShadowPointLights = ShadowPointLightIndex;
}

// ============================================================
// RenderGlobal — 뷰포트 루프 전 1회 Spot/Point shadow bake
// ============================================================

void FShadowMapPass::RenderGlobal(FD3DDevice& Device, FSystemResources& Resources, FScene& Scene, FSpatialPartition* Partition)
{
	ID3D11DeviceContext* DC = Device.GetDeviceContext();

	SetupShadowRenderState(Device, Resources, DC);

	FShadowMapResources& Res = Resources.ShadowResources;

	// Spot/Point shadow 렌더링
	RenderSpotShadows(DC, Device, Resources, Scene, Res, Partition);

	// Shadow depth 렌더링 종료 — DSV 언바인딩 (SRV 바인딩 전 R/W hazard 방지)
	DC->OMSetRenderTargets(0, nullptr, nullptr);

	// SRV 바인딩 + Shadow CB 업데이트
	BindShadowSRVs(DC, Res);
	UpdateShadowCB(DC, Resources, Res);
}
