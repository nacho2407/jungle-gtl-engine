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

REGISTER_RENDER_PASS(FShadowMapPass)

FShadowMapPass::FShadowMapPass()
{
	SpotLightAtlas.Init(4096.f, 64.f);
	PassType = ERenderPass::ShadowMap;
	// 커스텀 Execute — RenderState는 사용하지 않음 (직접 세팅)
}

FShadowMapPass::~FShadowMapPass()
{
	ReleaseResources();
	ShadowPerObjectCB.Release();
}

// ============================================================
// Shadow Map Texture 생성/관리
// ============================================================

void FShadowMapPass::EnsureCSM(ID3D11Device* Device, uint32 Resolution)
{
	if (Resources.CSMResolution == Resolution && Resources.CSMTexture) return;

	// 기존 CSM 리소스 해제
	if (Resources.CSMSRV) { Resources.CSMSRV->Release(); Resources.CSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (Resources.CSMDSV[i]) { Resources.CSMDSV[i]->Release(); Resources.CSMDSV[i] = nullptr; }
	}
	if (Resources.CSMTexture) { Resources.CSMTexture->Release(); Resources.CSMTexture = nullptr; }

	Resources.CSMResolution = Resolution;

	// Texture2DArray: ArraySize = MAX_SHADOW_CASCADES, R32_TYPELESS
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width  = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = MAX_SHADOW_CASCADES;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage  = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &Resources.CSMTexture);
	if (FAILED(hr)) return;

	// Per-cascade DSV (Texture2DArray slice)
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateDepthStencilView(Resources.CSMTexture, &DSVDesc, &Resources.CSMDSV[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = MAX_SHADOW_CASCADES;

	Device->CreateShaderResourceView(Resources.CSMTexture, &SRVDesc, &Resources.CSMSRV);

	// R32_FLOAT = 4 bytes per texel * cascades
	SHADOW_STATS_SET_MEMORY(static_cast<uint64>(Resolution) * Resolution * 4 * MAX_SHADOW_CASCADES);
	SHADOW_STATS_SET_RESOLUTION(Resolution);
}

void FShadowMapPass::EnsureSpotAtlas(ID3D11Device* Device, uint32 Resolution, uint32 PageCount)
{
	// TODO: Spot Atlas Texture2DArray 생성 (page 단위)
	(void)Device; (void)Resolution; (void)PageCount;
}

void FShadowMapPass::EnsurePointCube(ID3D11Device* Device, uint32 Resolution, uint32 CubeCount)
{
	// TODO: Point CubeMap TextureCubeArray 생성
	(void)Device; (void)Resolution; (void)CubeCount;
}

void FShadowMapPass::ReleaseResources()
{
	// CSM
	if (Resources.CSMSRV) { Resources.CSMSRV->Release(); Resources.CSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (Resources.CSMDSV[i]) { Resources.CSMDSV[i]->Release(); Resources.CSMDSV[i] = nullptr; }
	}
	if (Resources.CSMTexture) { Resources.CSMTexture->Release(); Resources.CSMTexture = nullptr; }

	// Spot Atlas
	if (Resources.SpotAtlasSRV) { Resources.SpotAtlasSRV->Release(); Resources.SpotAtlasSRV = nullptr; }
	if (Resources.SpotAtlasDSVs)
	{
		for (uint32 i = 0; i < Resources.SpotAtlasPageCount; ++i)
		{
			if (Resources.SpotAtlasDSVs[i]) Resources.SpotAtlasDSVs[i]->Release();
		}
		delete[] Resources.SpotAtlasDSVs;
		Resources.SpotAtlasDSVs = nullptr;
	}
	if (Resources.SpotAtlasTexture) { Resources.SpotAtlasTexture->Release(); Resources.SpotAtlasTexture = nullptr; }
	Resources.SpotAtlasPageCount = 0;

	// Point Cube
	if (Resources.PointCubeSRV) { Resources.PointCubeSRV->Release(); Resources.PointCubeSRV = nullptr; }
	if (Resources.PointCubeDSVs)
	{
		for (uint32 i = 0; i < Resources.PointCubeCount * 6; ++i)
		{
			if (Resources.PointCubeDSVs[i]) Resources.PointCubeDSVs[i]->Release();
		}
		delete[] Resources.PointCubeDSVs;
		Resources.PointCubeDSVs = nullptr;
	}
	if (Resources.PointCubeTexture) { Resources.PointCubeTexture->Release(); Resources.PointCubeTexture = nullptr; }
	Resources.PointCubeCount = 0;

	// StructuredBuffers
	if (Resources.SpotShadowDataSRV)    { Resources.SpotShadowDataSRV->Release();    Resources.SpotShadowDataSRV = nullptr; }
	if (Resources.SpotShadowDataBuffer) { Resources.SpotShadowDataBuffer->Release(); Resources.SpotShadowDataBuffer = nullptr; }
	Resources.SpotShadowDataCapacity = 0;

	if (Resources.PointShadowDataSRV)    { Resources.PointShadowDataSRV->Release();    Resources.PointShadowDataSRV = nullptr; }
	if (Resources.PointShadowDataBuffer) { Resources.PointShadowDataBuffer->Release(); Resources.PointShadowDataBuffer = nullptr; }
	Resources.PointShadowDataCapacity = 0;
}

void FShadowMapPass::BeginPass(const FPassContext& Ctx) {

}

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

	// Shadow map 리소스 확보 (CSM cascade 0을 legacy single-light 용도로 사용)
	const uint32 MapSize = FShadowSettings::Get().GetEffectiveResolution();
	EnsureCSM(Dev, MapSize);
	if (!Resources.CSMDSV[0]) { bHasValidShadow = false; return; }

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
		DC->ClearDepthStencilView(Resources.CSMDSV[0], D3D11_CLEAR_DEPTH, 0.0f, 0); // Reversed-Z: far=0
		DC->OMSetRenderTargets(0, nullptr, Resources.CSMDSV[0]);

		// Shadow map 뷰포트
		D3D11_VIEWPORT ShadowVP = {};
		ShadowVP.Width    = static_cast<float>(Resources.CSMResolution);
		ShadowVP.Height   = static_cast<float>(Resources.CSMResolution);
		ShadowVP.MinDepth = 0.0f;
		ShadowVP.MaxDepth = 1.0f;
		DC->RSSetViewports(1, &ShadowVP);

		// b0에 Light View/Proj 업로드
		//FFrameConstants LightFrameCB = {};
		//LightFrameCB.View       = LightView;
		//LightFrameCB.Projection = LightProj;
		//LightFrameCB.InvProj    = LightProj.GetInverse();
		//LightFrameCB.InvViewProj = LightViewProj.GetInverse();
		//
		//Ctx.Resources.FrameBuffer.Update(DC, &LightFrameCB, sizeof(FFrameConstants));

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
	if (Resources.CSMSRV)
	{
		DC->PSSetShaderResources(ESystemTexSlot::ShadowMap, 1, &Resources.CSMSRV);
	}
}
