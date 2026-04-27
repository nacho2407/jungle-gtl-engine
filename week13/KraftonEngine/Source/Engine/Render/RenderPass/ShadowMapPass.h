#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Types/ShadowSettings.h"
#include "Math/Matrix.h"

#include "Render/Resource/RenderResources.h"
#include "Render/Shadow/ShadowAtlasQuadTree.h"

struct FShadowMapResources;
class FSceneEnvironment;
class FPrimitiveSceneProxy;
class FSpatialPartition;

/*
	FShadowMapPass — 라이트 타입별 Shadow Depth 렌더링 패스.
	LightCulling(1)과 Opaque(2) 사이에 실행됩니다.

	GPU 리소스는 FSystemResources::ShadowResources에서 소유하며,
	이 패스는 리소스 Ensure + depth 렌더링 + SRV 바인딩만 담당합니다.

	구조:
	  BeginPass  — SRV 언바인딩, 리소스 Ensure, 공용 렌더 상태 세팅
	  Execute    — RenderDirectionalShadows / RenderSpotShadows / RenderPointShadows
	  EndPass    — 메인 RT/VP 복원, Shadow SRV 바인딩 (t21~t25), Shadow CB (b5) 업데이트
*/
class FShadowMapPass final : public FRenderPassBase
{
public:
	FShadowMapPass();
	~FShadowMapPass();

	// ── PSM 패스 인터페이스 (per-viewport, Directional 전용) ──
	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

	// ── Global Shadow (뷰포트 루프 전 1회, Spot/Point 전용) ──
	void RenderGlobal(FD3DDevice& Device, FSystemResources& Resources, FScene& Scene, FSpatialPartition* Partition = nullptr);

private:
	// ── 라이트 타입별 Shadow 렌더링 (팀원별 담당) ──
	void RenderDirectionalShadows(const FPassContext& Ctx, FShadowMapResources& Res);
	void RenderSpotShadows(ID3D11DeviceContext* DC, FD3DDevice& Device, FSystemResources& Resources, FScene& Scene, FShadowMapResources& Res, FSpatialPartition* Partition);
	void RenderSpotShadows(const FPassContext& Ctx, FShadowMapResources& Res);
	void RenderPointShadows(const FPassContext& Ctx, FShadowMapResources& Res);

	// ── 공용: frustum culling + depth-only draw ──
	void DrawShadowCasters(ID3D11DeviceContext* DC, FScene& Scene, const FConvexVolume& LightFrustum, FSpatialPartition* Partition = nullptr);

	// PSM용 래퍼 (기존 호출부 호환)
	void DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum);

	// ── 리소스 Ensure: FilterMode에 따라 depth-only / VSM moment 리소스 분기 ──
	void EnsureResources(const FPassContext& Ctx);

	// ── Shadow CB (b5) 업데이트 ──
	void UpdateShadowCB(ID3D11DeviceContext* DC, FSystemResources& Resources, FShadowMapResources& Res);
	void UpdateShadowCB(const FPassContext& Ctx);

	// ── 공용 렌더 상태 세팅 ──
	void SetupShadowRenderState(FD3DDevice& Device, FSystemResources& Resources, ID3D11DeviceContext* DC);

	// ── SRV 바인딩 ──
	void BindShadowSRVs(ID3D11DeviceContext* DC, FShadowMapResources& Res);

	// ── b2 (PerShader0)에 LightViewProj 업로드 ──
	void UploadLightViewProj(ID3D11DeviceContext* DC, const FMatrix& LightViewProj);

private:
	// Shadow 렌더링용 PerObject CB (b1) — Pass 전용 (light ViewProj 기준 Model 기록)
	FConstantBuffer ShadowPerObjectCB;

	// Light ViewProj CB (b2) — ShadowDepth 셰이더의 ShadowLightBuffer
	FConstantBuffer ShadowLightCB;

	// 이번 프레임 캐시
	EShadowFilterMode CurrentFilterMode = EShadowFilterMode::Hard;
	FShadowCBData     ShadowCBCache = {};

	FShadowAtlasQuadTree SpotLightAtlas;
};
