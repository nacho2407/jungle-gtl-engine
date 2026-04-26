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

	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

private:
	// ── 라이트 타입별 Shadow 렌더링 (팀원별 담당) ──
	void RenderDirectionalShadows(const FPassContext& Ctx, FShadowMapResources& Res);
	void RenderSpotShadows(const FPassContext& Ctx, FShadowMapResources& Res);
	void RenderPointShadows(const FPassContext& Ctx, FShadowMapResources& Res);

	// ── 공용: frustum culling + depth-only draw ──
	// ViewProj로 frustum을 만들고, 해당 frustum 안의 프록시를 depth-only 렌더링.
	// DSV/RTV, Viewport는 호출자가 이미 바인딩한 상태에서 호출.
	// VSM일 때는 PS가 moment 출력, Hard/PCF일 때는 PS=null.
	void DrawShadowCasters(const FPassContext& Ctx, const FConvexVolume& LightFrustum);

	// ── 리소스 Ensure: FilterMode에 따라 depth-only / VSM moment 리소스 분기 ──
	void EnsureResources(const FPassContext& Ctx);

	// ── Shadow CB (b5) 업데이트 ──
	void UpdateShadowCB(const FPassContext& Ctx);

private:
	// Shadow 렌더링용 PerObject CB (b1) — Pass 전용 (light ViewProj 기준 Model 기록)
	FConstantBuffer ShadowPerObjectCB;

	// 이번 프레임 캐시
	EShadowFilterMode CurrentFilterMode = EShadowFilterMode::Hard;
	FShadowCBData     ShadowCBCache = {};

	FShadowAtlasQuadTree SpotLightAtlas;
};
