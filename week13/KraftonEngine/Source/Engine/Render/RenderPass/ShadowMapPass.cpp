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
#include "Render/Types/ShadowSettings.h"
#include <d3d11.h>

REGISTER_RENDER_PASS(FShadowMapPass)

FShadowMapPass::FShadowMapPass()
{
	SpotLightAtlas.Init(4096.f, 64.f);
	PassType = ERenderPass::ShadowMap;
	// 커스텀 Execute — RenderState는 사용하지 않음 (직접 세팅)
}

FShadowMapPass::~FShadowMapPass()
{
	ShadowPerObjectCB.Release();
}

void FShadowMapPass::BeginPass(const FPassContext& Ctx) {}

// ============================================================
// Execute — 라이트별 Shadow Depth 렌더링
// ============================================================

void FShadowMapPass::Execute(const FPassContext& Ctx)
{
	if (!Ctx.Scene) return;

	const FSceneEnvironment& Env = Ctx.Scene->GetEnvironment();
	const uint32 NumSpots = Env.GetNumSpotLights();
	if (NumSpots == 0) { bHasValidShadow = false; return; }

	SCOPE_STAT_CAT("ShadowMapPass", "4_ExecutePass");
	GPU_SCOPE_STAT("ShadowMapPass");
	SHADOW_STATS_RESET();

	ID3D11Device* Dev = Ctx.Device.GetDevice();
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShadowMapResources& ShadowRes = Ctx.Resources.ShadowResources;

	// Shadow map 리소스 확보 (CSM cascade 0을 legacy single-light 용도로 사용)
	const uint32 MapSize = FShadowSettings::Get().GetEffectiveResolution();
	ShadowRes.EnsureCSM(Dev, MapSize);
	if (!ShadowRes.CSMDSV[0]) { bHasValidShadow = false; return; }

	// PerObject CB 생성 (한 번만)
	if (!ShadowPerObjectCB.GetBuffer())
		ShadowPerObjectCB.Create(Dev, sizeof(FPerObjectConstants));

	// --- 첫 번째 visible Spot Light로 shadow 렌더링 ---
	bHasValidShadow = false;

	for (uint32 li = 0; li < NumSpots; ++li)
	{
		const FSpotLightParams& Light = Env.GetSpotLight(li);
		if (!Light.bVisible) continue;

		// Light ViewProj + Frustum (유틸리티 사용)
		auto VP = FLightFrustumUtils::BuildSpotLightViewProj(Light);
		FMatrix LightView = VP.View;
		FMatrix LightProj = VP.Proj;
		LightViewProj = VP.ViewProj;

		FConvexVolume LightFrustum = FLightFrustumUtils::BuildSpotLightFrustum(Light);

		// 이전 프레임에서 t21에 바인딩된 SRV 해제 (DSV와 동일 리소스 → R/W hazard 방지)
		ID3D11ShaderResourceView* nullSRV = nullptr;
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMap, 1, &nullSRV);

		// Shadow DSV 클리어 + 바인딩 (CSM cascade 0을 legacy spot 용도로 사용)
		DC->ClearDepthStencilView(ShadowRes.CSMDSV[0], D3D11_CLEAR_DEPTH, 0.0f, 0); // Reversed-Z: far=0
		DC->OMSetRenderTargets(0, nullptr, ShadowRes.CSMDSV[0]);

		// Shadow map 뷰포트
		D3D11_VIEWPORT ShadowVP = {};
		ShadowVP.Width    = static_cast<float>(ShadowRes.CSMResolution);
		ShadowVP.Height   = static_cast<float>(ShadowRes.CSMResolution);
		ShadowVP.MinDepth = 0.0f;
		ShadowVP.MaxDepth = 1.0f;
		DC->RSSetViewports(1, &ShadowVP);

		// 렌더 상태: depth write + no color (PreDepth와 동일)
		Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::Default);
		Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::NoColor);
		Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidBackCull);
		DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		SHADOW_STATS_ADD_SHADOW_LIGHT(SpotLight);

		// 프록시 순회 — frustum culling + depth-only 렌더링
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

			// VS + InputLayout 바인딩 (PS는 즉시 해제)
			if (ProxyShader != LastShader)
			{
				ProxyShader->Bind(DC);
				DC->PSSetShader(nullptr, nullptr, 0);
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
			SHADOW_STATS_ADD_CASTER(SpotLight, 1);
		}

		bHasValidShadow = true;
		break; // v1: 첫 번째 visible spot light만
	}
}

// ============================================================
// EndPass — 카메라 View/Proj 복원 + 메인 RT 복원
// ============================================================

void FShadowMapPass::EndPass(const FPassContext& Ctx)
{
	if (!bHasValidShadow) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FShadowMapResources& ShadowRes = Ctx.Resources.ShadowResources;

	// 메인 RT/DSV 복원 (ShadowDSV를 output에서 해제해야 SRV 바인딩 가능)
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

	// Shadow Map SRV를 t21에 바인딩 (ShadowDSV가 output에서 해제된 후)
	if (ShadowRes.CSMSRV)
	{
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMap, 1, &ShadowRes.CSMSRV);
	}
}
