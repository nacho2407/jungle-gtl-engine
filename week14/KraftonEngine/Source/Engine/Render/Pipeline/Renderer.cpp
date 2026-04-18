#include "Renderer.h"

#include <functional>
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Render/Proxy/FScene.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Materials/MaterialManager.h"

// ============================================================
// FPassEvent — 패스 루프 내 Pre/Post 이벤트 훅
// 특정 패스 조건이 만족되면 콜백을 실행합니다.
// ============================================================
enum class EPassCompare : uint8 { Equal, Less, Greater, LessEqual, GreaterEqual };

struct FPassEvent
{
	ERenderPass    Pass;
	EPassCompare   Compare;
	bool           bOnce;
	bool           bExecuted = false;
	std::function<void()> Fn;

	bool TryExecute(ERenderPass CurPass)
	{
		if (bOnce && bExecuted) return false;

		bool bMatch = false;
		switch (Compare)
		{
		case EPassCompare::Equal:        bMatch = (CurPass == Pass); break;
		case EPassCompare::Less:         bMatch = ((uint32)CurPass < (uint32)Pass); break;
		case EPassCompare::Greater:      bMatch = ((uint32)CurPass > (uint32)Pass); break;
		case EPassCompare::LessEqual:    bMatch = ((uint32)CurPass <= (uint32)Pass); break;
		case EPassCompare::GreaterEqual: bMatch = ((uint32)CurPass >= (uint32)Pass); break;
		}

		if (bMatch) { Fn(); if (bOnce) bExecuted = true; }
		return bMatch;
	}
};


void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		UE_LOG("Failed to create D3D Device.");
	}

	FShaderManager::Get().Initialize(Device.GetDevice());
	FConstantBufferPool::Get().Initialize(Device.GetDevice());
	Resources.Create(Device.GetDevice());

	InitializePassRenderStates();

	Builder.Create(Device.GetDevice(), Device.GetDeviceContext(), PassRenderStates);

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	FGPUProfiler::Get().Shutdown();

	Builder.Release();

	Resources.Release();
	FConstantBufferPool::Get().Release();
	FShaderManager::Get().Release();
	FMaterialManager::Get().Release();
	Device.Release();
}

//	스왑체인 백버퍼 복귀 — ImGui 합성 직전에 호출
void FRenderer::BeginFrame()
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
	ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

	Context->ClearRenderTargetView(RTV, Device.GetClearColor());
	Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

	const D3D11_VIEWPORT& Viewport = Device.GetViewport();
	Context->RSSetViewports(1, &Viewport);
	Context->OMSetRenderTargets(1, &RTV, DSV);
}

// ============================================================
// Render — 정렬 + GPU 제출
// BeginCollect + Collector + BuildDynamicCommands 이후에 호출.
// ============================================================
void FRenderer::Render(const FFrameContext& Frame, FScene& Scene)
{
	FDrawCallStats::Reset();

	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	{
		SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
		Resources.UpdateFrameBuffer(Ctx, Frame);
		Resources.UpdateLightBuffer(Device.GetDevice(), Ctx, Scene);
	}

	// 시스템 샘플러 영구 바인딩 (s0-s2)
	Resources.BindSystemSamplers(Ctx);

	FDrawCommandList& CommandList = Builder.GetCommandList();

	// 커맨드 정렬 + 패스별 오프셋 빌드
	CommandList.Sort();

	// 단일 StateCache — 패스 간 상태 유지 (DSV Read-Only 전환 등)
	FStateCache Cache;
	Cache.Reset();
	Cache.RTV = Frame.ViewportRTV;
	Cache.DSV = Frame.ViewportDSV;

	// ── Pre/Post 패스 이벤트 등록 ──
	TArray<FPassEvent> PrePassEvents;
	TArray<FPassEvent> PostPassEvents;
	BuildPassEvents(PrePassEvents, PostPassEvents, Frame, Cache);

	// ── 패스 루프 ──
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		for (auto& PrePassEvent : PrePassEvents)
		{
			PrePassEvent.TryExecute(CurPass);
		}

		uint32 Start, End;
		CommandList.GetPassRange(CurPass, Start, End);
		if (Start >= End) continue;

		const char* PassName = GetRenderPassName(CurPass);
		SCOPE_STAT_CAT(PassName, "4_ExecutePass");
		GPU_SCOPE_STAT(PassName);

		CommandList.SubmitRange(Start, End, Resources, Ctx, Cache);

		for (auto& PostPassEvent : PostPassEvents)
		{
			PostPassEvent.TryExecute(CurPass);
		}
	}

	CleanupPassState(Cache);
}

// ============================================================
// CleanupPassState — 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
// ============================================================
void FRenderer::CleanupPassState(FStateCache& Cache)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	Resources.UnbindSystemTextures(Ctx);

	Cache.Cleanup(Ctx);
	Builder.GetCommandList().Reset();
}

// ============================================================
// BuildPassEvents — 패스 루프 Pre/Post 이벤트 등록
// ============================================================
void FRenderer::BuildPassEvents(TArray<FPassEvent>& PrePassEvents,
	TArray<FPassEvent>& PostPassEvents,
	const FFrameContext& Frame, FStateCache& Cache)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	// PreDepth 진입: RTV 해제 → DSV만 바인딩 (depth만 기록, 색 출력 없음)
	PrePassEvents.push_back({ ERenderPass::PreDepth, EPassCompare::Equal, true, false,
		[Context, &Cache]()
		{
			Context->OMSetRenderTargets(0, nullptr, Cache.DSV);
			Cache.bForceAll = true;
		}
		});

	// PreDepth 완료: RTV 복귀
	PostPassEvents.push_back({ ERenderPass::PreDepth, EPassCompare::Equal, true, false,
		[Context, &Frame, &Cache]()
		{
			Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
			Cache.bForceAll = true;
		}
		});

	// CopyDepth: Opaque 진입 전 Depth 복사 → MRT 세팅 → SceneDepth SRV 바인딩
	if (Frame.DepthTexture && Frame.DepthCopyTexture)
	{
		PrePassEvents.push_back({ ERenderPass::Opaque, EPassCompare::GreaterEqual, true, false,
			[Context, &Frame, &Cache]()
			{
				Context->OMSetRenderTargets(0, nullptr, nullptr);
				Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

				// MRT: Opaque 패스에서 Normal RT를 SV_TARGET1로 사용
				if (Frame.NormalRTV)
				{
					ID3D11RenderTargetView* RTVs[2] = { Cache.RTV, Frame.NormalRTV };
					Context->OMSetRenderTargets(2, RTVs, Cache.DSV);
				}
				else
				{
					Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
				}

				ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
				Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);

				Cache.bForceAll = true;
			}
			});
	}

	// Opaque 완료 후: MRT 해제 → 1 RTV 복귀 + Normal SRV 바인딩
	if (Frame.NormalRTV)
	{
		PostPassEvents.push_back({ ERenderPass::Opaque, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache]()
			{
				// MRT 해제 → RTV 1개로 복귀 (Normal RT를 SRV로 읽기 위해 RTV에서 분리)
				Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

				// Normal SRV 바인딩
				if (Frame.NormalSRV)
				{
					ID3D11ShaderResourceView* normalSRV = Frame.NormalSRV;
					Context->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &normalSRV);
				}

				Cache.bForceAll = true;
			}
			});
	}

	// CopyStencil: SelectionMask 완료 후(PostProcess 진입 전) Stencil 복사 → Outline에서 사용
	if (Frame.DepthTexture && Frame.DepthCopyTexture && Frame.StencilCopySRV)
	{
		PrePassEvents.push_back({ ERenderPass::PostProcess, EPassCompare::GreaterEqual, true, false,
			[Context, &Frame, &Cache]()
			{
				Context->OMSetRenderTargets(0, nullptr, nullptr);
				Context->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
				Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

				ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
				Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);

				Cache.bForceAll = true;
			}
			});
	}

	// CopySceneColor: FXAA 패스 진입 전 현재 화면 복사 → SceneColorCopySRV로 읽기
	if (Frame.SceneColorCopyTexture && Frame.ViewportRenderTexture)
	{
		PrePassEvents.push_back({ ERenderPass::FXAA, EPassCompare::Equal, true, false,
			[Context, &Frame, &Cache]()
			{
				Context->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
				Context->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

				ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
				Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);

				Cache.bForceAll = true;
			}
			});
	}
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	//                              DepthStencil                         Blend                Rasterizer                   Topology                                WireframeAware
	S[(uint32)E::PreDepth] = { EDepthStencilState::Default,           EBlendState::NoColor,    ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Opaque] = { EDepthStencilState::DepthGreaterEqual, EBlendState::Opaque,     ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::AlphaBlend] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Decal] = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::AdditiveDecal] = { EDepthStencilState::DepthReadOnly, EBlendState::Additive, ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite, EBlendState::NoColor,    ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::EditorLines] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     false };
	S[(uint32)E::PostProcess] = { EDepthStencilState::NoDepth,      EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::FXAA] = { EDepthStencilState::NoDepth,      EBlendState::Opaque,     ERasterizerState::SolidNoCull,   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoOuter] = { EDepthStencilState::GizmoOutside, EBlendState::Opaque,     ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoInner] = { EDepthStencilState::GizmoInside,  EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::OverlayFont] = { EDepthStencilState::NoDepth,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}

