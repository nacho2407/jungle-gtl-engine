#include "PassEventBuilder.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/RenderConstants.h"

// ============================================================
// Build — 모든 패스 이벤트 등록 진입점
// ============================================================
void FPassEventBuilder::Build(FD3DDevice& Device,
	const FFrameContext& Frame, FStateCache& Cache,
	TArray<FPassEvent>& OutPreEvents,
	TArray<FPassEvent>& OutPostEvents)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	RegisterPreDepthEvents(Ctx, Frame, Cache, OutPreEvents, OutPostEvents);
	RegisterDepthCopyAndMRTEvents(Ctx, Frame, Cache, OutPreEvents, OutPostEvents);
	RegisterStencilCopyEvents(Ctx, Frame, Cache, OutPreEvents);
	RegisterSceneColorCopyEvents(Ctx, Frame, Cache, OutPreEvents);
}

// ============================================================
// PreDepth: DSV-only 렌더링 (색 출력 없음) → RTV 복귀
// ============================================================
void FPassEventBuilder::RegisterPreDepthEvents(ID3D11DeviceContext* Ctx,
	const FFrameContext& Frame, FStateCache& Cache,
	TArray<FPassEvent>& Pre, TArray<FPassEvent>& Post)
{
	// Pre: RTV 해제 → DSV만 바인딩
	Pre.push_back({ ERenderPass::PreDepth, EPassCompare::Equal, true, false,
		[Ctx, &Cache]()
		{
			Ctx->OMSetRenderTargets(0, nullptr, Cache.DSV);
			Cache.bForceAll = true;
		}
		});

	// Post: RTV 복귀
	Post.push_back({ ERenderPass::PreDepth, EPassCompare::Equal, true, false,
		[Ctx, &Frame, &Cache]()
		{
			Ctx->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
			Cache.bForceAll = true;
		}
		});
}

// ============================================================
// DepthCopy + MRT: Opaque 진입 전 Depth 복사, Normal MRT 설정/해제
// ============================================================
void FPassEventBuilder::RegisterDepthCopyAndMRTEvents(ID3D11DeviceContext* Ctx,
	const FFrameContext& Frame, FStateCache& Cache,
	TArray<FPassEvent>& Pre, TArray<FPassEvent>& Post)
{
	// Pre-Opaque: Depth 복사 → MRT 세팅 → SceneDepth SRV 바인딩
	if (Frame.DepthTexture && Frame.DepthCopyTexture)
	{
		Pre.push_back({ ERenderPass::Opaque, EPassCompare::GreaterEqual, true, false,
			[Ctx, &Frame, &Cache]()
			{
				Ctx->OMSetRenderTargets(0, nullptr, nullptr);
				Ctx->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

				// MRT: Normal RT를 SV_TARGET1로 사용
				if (Frame.NormalRTV)
				{
					ID3D11RenderTargetView* RTVs[2] = { Cache.RTV, Frame.NormalRTV };
					Ctx->OMSetRenderTargets(2, RTVs, Cache.DSV);
				}
				else
				{
					Ctx->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
				}

				ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
				Ctx->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);

				Cache.bForceAll = true;
			}
			});
	}

	// Post-Opaque: MRT 해제 → 1 RTV 복귀 + Normal SRV 바인딩
	if (Frame.NormalRTV)
	{
		Post.push_back({ ERenderPass::Opaque, EPassCompare::Equal, true, false,
			[Ctx, &Frame, &Cache]()
			{
				Ctx->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

				if (Frame.NormalSRV)
				{
					ID3D11ShaderResourceView* normalSRV = Frame.NormalSRV;
					Ctx->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &normalSRV);
				}

				Cache.bForceAll = true;
			}
			});
	}
}

// ============================================================
// StencilCopy: SelectionMask 완료 후 Stencil 복사 → Outline에서 사용
// ============================================================
void FPassEventBuilder::RegisterStencilCopyEvents(ID3D11DeviceContext* Ctx,
	const FFrameContext& Frame, FStateCache& Cache,
	TArray<FPassEvent>& Pre)
{
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.StencilCopySRV)
		return;

	Pre.push_back({ ERenderPass::PostProcess, EPassCompare::GreaterEqual, true, false,
		[Ctx, &Frame, &Cache]()
		{
			Ctx->OMSetRenderTargets(0, nullptr, nullptr);
			Ctx->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
			Ctx->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

			ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
			Ctx->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);

			Cache.bForceAll = true;
		}
		});
}

// ============================================================
// SceneColorCopy: FXAA 진입 전 현재 화면 복사
// ============================================================
void FPassEventBuilder::RegisterSceneColorCopyEvents(ID3D11DeviceContext* Ctx,
	const FFrameContext& Frame, FStateCache& Cache,
	TArray<FPassEvent>& Pre)
{
	if (!Frame.SceneColorCopyTexture || !Frame.ViewportRenderTexture)
		return;

	Pre.push_back({ ERenderPass::FXAA, EPassCompare::Equal, true, false,
		[Ctx, &Frame, &Cache]()
		{
			Ctx->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
			Ctx->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

			ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
			Ctx->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);

			Cache.bForceAll = true;
		}
		});
}
