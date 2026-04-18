#include "Renderer.h"

#include <algorithm>
#include <functional>
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/FogParams.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Materials/MaterialManager.h"
#include "Engine/Render/Pipeline/ForwardLightData.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

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

	EditorLines.Create(Device.GetDevice());
	GridLines.Create(Device.GetDevice());
	FontGeometry.Create(Device.GetDevice());

	InitializePassRenderStates();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	FGPUProfiler::Get().Shutdown();

	EditorLines.Release();
	GridLines.Release();
	FontGeometry.Release();

	for (FConstantBuffer& CB : PerObjectCBPool)
	{
		CB.Release();
	}
	PerObjectCBPool.clear();

	Resources.Release();
	FConstantBufferPool::Get().Release();
	FShaderManager::Get().Release();
	FMaterialManager::Get().Release();
	Device.Release();
}

// ============================================================
// BeginCollect — DrawCommandList + 동적 지오메트리 초기화
// Collector가 BuildCommandForProxy/AddWorldText를 호출하기 전에 반드시 호출
// ============================================================
void FRenderer::BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount)
{
	DrawCommandList.Reset();
	CollectViewMode = Frame.ViewMode;
	bHasSelectionMaskCommands = false;

	// PerObjectCBPool 미리 할당 — Collect 도중 resize로 FDrawCommand.PerObjectCB
	// 포인터가 무효화되는 것을 방지
	if (MaxProxyCount > 0)
		EnsurePerObjectCBPoolCapacity(MaxProxyCount);

	// 동적 지오메트리 초기화
	EditorLines.Clear();
	GridLines.Clear();
	FontGeometry.Clear();
	FontGeometry.ClearScreen();

	if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
		FontGeometry.EnsureCharInfoMap(FontRes);
}

// ============================================================
// BuildCommandForProxy — Collector가 직접 호출하여 Proxy → FDrawCommand 변환
// ============================================================
void FRenderer::BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass)
{
	if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid()) return;

	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	const FPassRenderState& PassState = PassRenderStates[(uint32)Pass];

	// Wireframe 모드 처리
	ERasterizerState Rasterizer = PassState.Rasterizer;
	if (PassState.bWireframeAware && CollectViewMode == EViewMode::Wireframe)
		Rasterizer = ERasterizerState::WireFrame;

	// PerObjectCB 업데이트
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}

	// PerShaderCB 업데이트 (Gizmo, SubUV, Decal 등) — lazy creation if buffer not yet allocated
	if (Proxy.ExtraCB.Buffer)
	{
		if (!Proxy.ExtraCB.Buffer->GetBuffer())
			Proxy.ExtraCB.Buffer->Create(Device.GetDevice(), Proxy.ExtraCB.Size);
		Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);
	}

	// SelectionMask 커맨드 존재 추적
	if (Pass == ERenderPass::SelectionMask)
		bHasSelectionMaskCommands = true;

	// ViewMode에 따른 UberLit 셰이더 변형 선택
	// Wireframe 등 비-Lit 모드에서는 원본 StaticMesh 셰이더 유지
	FShader* EffectiveShader = Proxy.Shader;
	if (Proxy.Shader == FShaderManager::Get().GetShader(EShaderType::StaticMesh))
	{
		switch (CollectViewMode)
		{
		case EViewMode::Lit_Gouraud:
			EffectiveShader = FShaderManager::Get().GetShader(EShaderType::UberLit_Gouraud);
			break;
		case EViewMode::Lit_Lambert:
			EffectiveShader = FShaderManager::Get().GetShader(EShaderType::UberLit_Lambert);
			break;
		case EViewMode::Lit_Phong:
			EffectiveShader = FShaderManager::Get().GetShader(EShaderType::UberLit_Phong);
			break;
		default:
			break; // Wireframe, SceneDepth, WorldNormal 등 — 원본 셰이더 유지
		}
	}

	// Proxy.ExtraCB → PerShaderCB 인덱스 변환 헬퍼
	auto SetProxyExtraCB = [&](FDrawCommand& Cmd)
		{
			if (Proxy.ExtraCB.Buffer)
			{
				const uint32 Idx = Proxy.ExtraCB.Slot - ECBSlot::PerShader0;
				check(Idx < 2);
				Cmd.Bindings.PerShaderCB[Idx] = Proxy.ExtraCB.Buffer;
			}
		};

	const bool bDepthOnly = (Pass == ERenderPass::PreDepth);
	const bool bApplyMaterialState = !bDepthOnly && (Pass == Proxy.Pass);

	// MeshBuffer → FDrawCommandBuffer 변환
	FDrawCommandBuffer ProxyBuffer;
	ProxyBuffer.VB       = Proxy.MeshBuffer->GetVertexBuffer().GetBuffer();
	ProxyBuffer.VBStride = Proxy.MeshBuffer->GetVertexBuffer().GetStride();
	ProxyBuffer.IB       = Proxy.MeshBuffer->GetIndexBuffer().GetBuffer();

	// 커맨드 공통 초기화 람다
	auto InitCommand = [&](FDrawCommand& Cmd)
		{
			Cmd.Shader       = EffectiveShader;
			Cmd.RenderState.Blend        = PassState.Blend;
			Cmd.RenderState.DepthStencil = PassState.DepthStencil;
			Cmd.RenderState.Rasterizer   = Rasterizer;
			Cmd.RenderState.Topology     = PassState.Topology;
			Cmd.Buffer       = ProxyBuffer;
			Cmd.PerObjectCB  = PerObjCB;
			Cmd.Pass         = Pass;
		};

	// SectionDraws가 있으면 섹션당 1개 커맨드, 없으면 1개 커맨드
	if (!Proxy.SectionDraws.empty())
	{
		for (const FMeshSectionDraw& Section : Proxy.SectionDraws)
		{
			if (Section.IndexCount == 0) continue;
			if (!ProxyBuffer.IB) continue;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			InitCommand(Cmd);
			Cmd.Buffer.FirstIndex = Section.FirstIndex;
			Cmd.Buffer.IndexCount = Section.IndexCount;

			if (!bDepthOnly && Section.Material)
			{
				UMaterial* Mat = Section.Material;

				// dirty CB 업로드
				Mat->FlushDirtyBuffers(Ctx);

				Cmd.Bindings.PerShaderCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
				Cmd.Bindings.PerShaderCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);
				SetProxyExtraCB(Cmd);

				// CachedSRVs에서 직접 복사 (map lookup 회피)
				const ID3D11ShaderResourceView* const* MatSRVs = Mat->GetCachedSRVs();
				for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
					Cmd.Bindings.SRVs[s] = const_cast<ID3D11ShaderResourceView*>(MatSRVs[s]);

				// Material 렌더 상태 오버라이드 (메인 패스에서만)
				if (bApplyMaterialState)
				{
					Cmd.RenderState.Blend = Mat->GetBlendState();
					Cmd.RenderState.DepthStencil = Mat->GetDepthStencilState();
					Cmd.RenderState.Rasterizer = (PassState.bWireframeAware && CollectViewMode == EViewMode::Wireframe)
						? ERasterizerState::WireFrame
						: Mat->GetRasterizerState();
				}
			}
			else if (!bDepthOnly)
			{
				SetProxyExtraCB(Cmd);
			}

			Cmd.BuildSortKey();
		}
	}
	else
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		InitCommand(Cmd);

		// MeshBuffer 전체 드로우 — IndexCount/VertexCount 명시
		if (ProxyBuffer.IB)
			Cmd.Buffer.IndexCount = Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount();
		else
			Cmd.Buffer.VertexCount = Proxy.MeshBuffer->GetVertexBuffer().GetVertexCount();

		if (!bDepthOnly)
		{
			SetProxyExtraCB(Cmd);
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = Proxy.DiffuseSRV;
		}

		// Material 렌더 상태 오버라이드 (메인 패스에서만)
		if (bApplyMaterialState && Proxy.Material)
		{
			Cmd.RenderState.Blend = Proxy.Material->GetBlendState();
			Cmd.RenderState.DepthStencil = Proxy.Material->GetDepthStencilState();
			Cmd.RenderState.Rasterizer = (PassState.bWireframeAware && CollectViewMode == EViewMode::Wireframe)
				? ERasterizerState::WireFrame
				: Proxy.Material->GetRasterizerState();
		}

		Cmd.BuildSortKey();
	}
}

void FRenderer::BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy)
{
	if (!ReceiverProxy.MeshBuffer || !ReceiverProxy.MeshBuffer->IsValid()) return;
	if (!DecalProxy.Shader || !DecalProxy.DiffuseSRV) return;

	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	const ERenderPass DecalPass = DecalProxy.Pass;
	const FPassRenderState& PassState = PassRenderStates[(uint32)DecalPass];

	ERasterizerState Rasterizer = PassState.Rasterizer;
	if (PassState.bWireframeAware && CollectViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	FConstantBuffer* ReceiverPerObjCB = GetPerObjectCBForProxy(ReceiverProxy);
	if (ReceiverPerObjCB && ReceiverProxy.NeedsPerObjectCBUpload())
	{
		ReceiverPerObjCB->Update(Ctx, &ReceiverProxy.PerObjectConstants, sizeof(FPerObjectConstants));
		ReceiverProxy.ClearPerObjectCBDirty();
	}

	if (DecalProxy.ExtraCB.Buffer)
	{
		if (!DecalProxy.ExtraCB.Buffer->GetBuffer())
		{
			DecalProxy.ExtraCB.Buffer->Create(Device.GetDevice(), DecalProxy.ExtraCB.Size);
		}
		DecalProxy.ExtraCB.Buffer->Update(Ctx, DecalProxy.ExtraCB.Data, DecalProxy.ExtraCB.Size);
	}

	FDrawCommandBuffer ReceiverBuffer;
	ReceiverBuffer.VB       = ReceiverProxy.MeshBuffer->GetVertexBuffer().GetBuffer();
	ReceiverBuffer.VBStride = ReceiverProxy.MeshBuffer->GetVertexBuffer().GetStride();
	ReceiverBuffer.IB       = ReceiverProxy.MeshBuffer->GetIndexBuffer().GetBuffer();

	auto AddDraw = [&](uint32 FirstIndex, uint32 IndexCount)
		{
			if (IndexCount == 0) return;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Shader = DecalProxy.Shader;

			// 머티리얼 기반 렌더 상태 적용
			if (DecalProxy.Material)
			{
				Cmd.RenderState.Blend = DecalProxy.Material->GetBlendState();
				Cmd.RenderState.DepthStencil = DecalProxy.Material->GetDepthStencilState();
				Cmd.RenderState.Rasterizer = DecalProxy.Material->GetRasterizerState();
			}
			else
			{
				Cmd.RenderState.Blend = PassState.Blend;
				Cmd.RenderState.DepthStencil = PassState.DepthStencil;
				Cmd.RenderState.Rasterizer = Rasterizer;
			}

			Cmd.RenderState.Topology = PassState.Topology;
			Cmd.Buffer = ReceiverBuffer;
			Cmd.Buffer.FirstIndex = FirstIndex;
			Cmd.Buffer.IndexCount = IndexCount;
			Cmd.PerObjectCB = ReceiverPerObjCB;
			Cmd.Bindings.PerShaderCB[0] = DecalProxy.ExtraCB.Buffer;
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = DecalProxy.DiffuseSRV;
			Cmd.Pass = DecalPass;
			Cmd.BuildSortKey();
		};

	if (!ReceiverProxy.SectionDraws.empty())
	{
		for (const FMeshSectionDraw& Section : ReceiverProxy.SectionDraws)
		{
			AddDraw(Section.FirstIndex, Section.IndexCount);
		}
	}
	else if (ReceiverBuffer.IB)
	{
		AddDraw(0, ReceiverProxy.MeshBuffer->GetIndexBuffer().GetIndexCount());
	}
}

// ============================================================
// AddWorldText — Collector가 Font 프록시를 배칭할 때 호출
// ============================================================
void FRenderer::AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame)
{
	FontGeometry.AddWorldText(
		TextProxy->CachedText,
		TextProxy->CachedBillboardMatrix.GetLocation(),
		Frame.CameraRight,
		Frame.CameraUp,
		TextProxy->CachedBillboardMatrix.GetScale(),
		TextProxy->CachedFontScale
	);
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
// BuildDynamicCommands — Collect 마무리: FScene 경량 데이터 → 동적 지오메트리 → FDrawCommand
// Pipeline의 Collect 블록 끝에서 호출.
// ============================================================
void FRenderer::BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene)
{
	PrepareDynamicGeometry(Frame, Scene);
	BuildDynamicDrawCommands(Frame, Device.GetDeviceContext(), Scene);
}

// ============================================================
// Render — 정렬 + GPU 제출
// BeginCollect + Collector + BuildDynamicCommands 이후에 호출.
// ============================================================
void FRenderer::Render(const FFrameContext& Frame, FScene& Scene)
{
	FDrawCallStats::Reset();

	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	{
		SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
		UpdateFrameBuffer(Context, Frame);
		UpdateLightBuffer(Device.GetDevice(), Context, Scene);
	}

	// 시스템 샘플러 영구 바인딩 (s0-s2)
	Resources.BindSystemSamplers(Context);

	// 커맨드 정렬 + 패스별 오프셋 빌드
	DrawCommandList.Sort();

	// 단일 StateCache — 패스 간 상태 유지 (DSV Read-Only 전환 등)
	FStateCache Cache;
	Cache.Reset();
	Cache.RTV = Frame.ViewportRTV;
	Cache.DSV = Frame.ViewportDSV;

	// ── Pre/Post 패스 이벤트 등록 ──
	TArray<FPassEvent> PrePassEvents;
	TArray<FPassEvent> PostPassEvents;
	BuildPassEvents(PrePassEvents, PostPassEvents, Context, Frame, Cache);

	// ── 패스 루프 ──
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		for (auto& PrePassEvent : PrePassEvents)
		{
			PrePassEvent.TryExecute(CurPass);
		}

		uint32 Start, End;
		DrawCommandList.GetPassRange(CurPass, Start, End);
		if (Start >= End) continue;

		const char* PassName = GetRenderPassName(CurPass);
		SCOPE_STAT_CAT(PassName, "4_ExecutePass");
		GPU_SCOPE_STAT(PassName);

		DrawCommandList.SubmitRange(Start, End, Device, Context, Cache);

		for (auto& PostPassEvent : PostPassEvents)
		{
			PostPassEvent.TryExecute(CurPass);
		}
	}

	CleanupPassState(Context, Cache);
}

// ============================================================
// CleanupPassState — 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
// ============================================================
void FRenderer::CleanupPassState(ID3D11DeviceContext* Context, FStateCache& Cache)
{
	// 시스템 텍스처 언바인딩
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &nullSRV);
	Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &nullSRV);
	Context->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &nullSRV);
	Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &nullSRV);

	Cache.Cleanup(Context);
	DrawCommandList.Reset();
}

// ============================================================
// BuildPassEvents — 패스 루프 Pre/Post 이벤트 등록
// ============================================================
void FRenderer::BuildPassEvents(TArray<FPassEvent>& PrePassEvents,
	TArray<FPassEvent>& PostPassEvents,
	ID3D11DeviceContext* Context, const FFrameContext& Frame, FStateCache& Cache)
{
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
// PrepareDynamicGeometry — FScene의 경량 데이터 → 라인/폰트 지오메트리
// ============================================================
void FRenderer::PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene)
{
	if (!Scene) return;

	// --- Editor 패스: AABB 디버그 박스 + DebugDraw 라인 ---
	for (const auto& AABB : Scene->GetDebugAABBs())
	{
		EditorLines.AddAABB(FBoundingBox{ AABB.Min, AABB.Max }, AABB.Color);
	}
	for (const auto& Line : Scene->GetDebugLines())
	{
		EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
	}

	// --- Grid 패스: 월드 그리드 + 축 ---
	if (Scene->HasGrid())
	{
		const FVector CameraPos = Frame.View.GetInverseFast().GetLocation();
		FVector CameraFwd = Frame.CameraRight.Cross(Frame.CameraUp);
		CameraFwd.Normalize();

		GridLines.AddWorldHelpers(
			Frame.ShowFlags,
			Scene->GetGridSpacing(),
			Scene->GetGridHalfLineCount(),
			CameraPos, CameraFwd, Frame.IsFixedOrtho());
	}

	// --- OverlayFont 패스: 스크린 공간 텍스트 ---
	for (const auto& Text : Scene->GetOverlayTexts())
	{
		if (!Text.Text.empty())
		{
			FontGeometry.AddScreenText(
				Text.Text,
				Text.Position.X,
				Text.Position.Y,
				Frame.ViewportWidth,
				Frame.ViewportHeight,
				Text.Scale
			);
		}
	}
}

// ============================================================
// Dynamic geometry → FDrawCommand 변환 (Font, Line)
// ============================================================
void FRenderer::BuildDynamicDrawCommands(const FFrameContext& Frame, ID3D11DeviceContext* Ctx, const FScene* CollectScene)
{
	EViewMode ViewMode = Frame.ViewMode;

	auto ApplyPassState = [&](FDrawCommand& Cmd, ERenderPass Pass)
		{
			const FPassRenderState& S = PassRenderStates[(uint32)Pass];
			Cmd.RenderState.DepthStencil = S.DepthStencil;
			Cmd.RenderState.Blend = S.Blend;
			Cmd.RenderState.Rasterizer = S.Rasterizer;
			Cmd.RenderState.Topology = S.Topology;
			Cmd.Pass = Pass;

			if (S.bWireframeAware && ViewMode == EViewMode::Wireframe)
				Cmd.RenderState.Rasterizer = ERasterizerState::WireFrame;
		};

	// --- Editor Lines + Grid Lines → EditorLines 패스 ---
	FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);

	if (EditorLines.GetLineCount() > 0 && EditorLines.UploadBuffers(Ctx))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::EditorLines);
		Cmd.Shader = EditorShader;
		Cmd.Buffer = { EditorLines.GetVBBuffer(), EditorLines.GetVBStride(), EditorLines.GetIBBuffer() };
		Cmd.Buffer.IndexCount = EditorLines.GetIndexCount();
		Cmd.BuildSortKey();
	}

	if (GridLines.GetLineCount() > 0 && GridLines.UploadBuffers(Ctx))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::EditorLines);
		Cmd.Shader = EditorShader;
		Cmd.Buffer = { GridLines.GetVBBuffer(), GridLines.GetVBStride(), GridLines.GetIBBuffer() };
		Cmd.Buffer.IndexCount = GridLines.GetIndexCount();
		Cmd.BuildSortKey();
	}

	// --- PostProcess: HeightFog → Outline (SortKey UserBits로 순서 보장) ---
	{
		const FPassRenderState& PPState = PassRenderStates[(uint32)ERenderPass::PostProcess];

		// HeightFog (UserBits=0 → Outline보다 먼저)
		if (Frame.ShowFlags.bFog && CollectScene && CollectScene->HasFog())
		{
			FShader* FogShader = FShaderManager::Get().GetShader(EShaderType::HeightFog);
			if (FogShader)
			{
				// Fog CB (b6) 업데이트
				FConstantBuffer* FogCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Fog, sizeof(FFogConstants));
				const FFogParams& FogParams = CollectScene->GetFogParams();
				FFogConstants fogData = {};
				fogData.InscatteringColor = FogParams.InscatteringColor;
				fogData.Density = FogParams.Density;
				fogData.HeightFalloff = FogParams.HeightFalloff;
				fogData.FogBaseHeight = FogParams.FogBaseHeight;
				fogData.StartDistance = FogParams.StartDistance;
				fogData.CutoffDistance = FogParams.CutoffDistance;
				fogData.MaxOpacity = FogParams.MaxOpacity;
				FogCB->Update(Ctx, &fogData, sizeof(FFogConstants));

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				Cmd.Shader = FogShader;
				Cmd.RenderState.DepthStencil = PPState.DepthStencil;
				Cmd.RenderState.Blend = PPState.Blend;
				Cmd.RenderState.Rasterizer = PPState.Rasterizer;
				Cmd.RenderState.Topology = PPState.Topology;
				Cmd.Buffer.VertexCount = 3;  // Fullscreen triangle (SV_VertexID)
				Cmd.Bindings.PerShaderCB[0] = FogCB;
				Cmd.Pass = ERenderPass::PostProcess;
				Cmd.BuildSortKey(0);
			}
		}

		// Outline (UserBits=1 → HeightFog 뒤)
		if (bHasSelectionMaskCommands)
		{
			FShader* PPShader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
			if (PPShader)
			{
				// Outline CB (b3) 업데이트
				FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Outline, sizeof(FOutlinePostProcessConstants));
				FOutlinePostProcessConstants ppConstants;
				ppConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
				ppConstants.OutlineThickness = 3.0f;
				OutlineCB->Update(Ctx, &ppConstants, sizeof(ppConstants));

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				Cmd.Shader = PPShader;
				Cmd.RenderState.DepthStencil = PPState.DepthStencil;
				Cmd.RenderState.Blend = PPState.Blend;
				Cmd.RenderState.Rasterizer = PPState.Rasterizer;
				Cmd.RenderState.Topology = PPState.Topology;
				Cmd.Buffer.VertexCount = 3;  // Fullscreen triangle (SV_VertexID)
				Cmd.Bindings.PerShaderCB[0] = OutlineCB;
				Cmd.Pass = ERenderPass::PostProcess;
				Cmd.BuildSortKey(1);
			}
		}

		// SceneDepth (UserBits=2 → Outline 뒤)
		if (CollectViewMode == EViewMode::SceneDepth)
		{
			FShader* DepthShader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
			if (DepthShader)
			{
				FConstantBuffer* SceneDepthCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::SceneDepth, sizeof(FSceneDepthPConstants));
				FViewportRenderOptions Opts = Frame.GetRenderOptions();
				FSceneDepthPConstants depthData = {};
				depthData.Exponent = Opts.Exponent;
				depthData.NearClip = Frame.NearClip;
				depthData.FarClip = Frame.FarClip;
				depthData.Mode = Opts.SceneDepthVisMode;
				SceneDepthCB->Update(Ctx, &depthData, sizeof(FSceneDepthPConstants));

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				Cmd.Shader = DepthShader;
				Cmd.RenderState.DepthStencil = PPState.DepthStencil;
				Cmd.RenderState.Blend = PPState.Blend;
				Cmd.RenderState.Rasterizer = PPState.Rasterizer;
				Cmd.RenderState.Topology = PPState.Topology;
				Cmd.Buffer.VertexCount = 3;
				Cmd.Bindings.PerShaderCB[0] = SceneDepthCB;
				Cmd.Pass = ERenderPass::PostProcess;
				Cmd.BuildSortKey(2);
			}
		}

		// WorldNormal (UserBits=3 → SceneDepth 뒤)
		if (CollectViewMode == EViewMode::WorldNormal)
		{
			FShader* NormalShader = FShaderManager::Get().GetShader(EShaderType::SceneNormal);
			if (NormalShader)
			{
				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				Cmd.Shader = NormalShader;
				Cmd.RenderState.DepthStencil = PPState.DepthStencil;
				Cmd.RenderState.Blend = PPState.Blend;
				Cmd.RenderState.Rasterizer = PPState.Rasterizer;
				Cmd.RenderState.Topology = PPState.Topology;
				Cmd.Buffer.VertexCount = 3;
				Cmd.Pass = ERenderPass::PostProcess;
				Cmd.BuildSortKey(3);
			}
		}

		if (Frame.ShowFlags.bFXAA)
		{
			FShader* FXAAShader = FShaderManager::Get().GetShader(EShaderType::FXAA);
			if (FXAAShader)
			{
				FConstantBuffer* FXAACB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::FXAA, sizeof(FFXAAConstants));
				FViewportRenderOptions Opts = Frame.GetRenderOptions();
				FFXAAConstants FXAAData = {};
				FXAAData.EdgeThreshold = Opts.EdgeThreshold;
				FXAAData.EdgeThresholdMin = Opts.EdgeThresholdMin;
				FXAACB->Update(Ctx, &FXAAData, sizeof(FFXAAConstants));

				const FPassRenderState& FXAAState = PassRenderStates[(uint32)ERenderPass::FXAA];
				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				Cmd.Shader = FXAAShader;
				Cmd.RenderState.DepthStencil = FXAAState.DepthStencil;
				Cmd.RenderState.Blend = FXAAState.Blend;
				Cmd.RenderState.Rasterizer = FXAAState.Rasterizer;
				Cmd.RenderState.Topology = FXAAState.Topology;
				Cmd.Buffer.VertexCount = 3;
				Cmd.Bindings.PerShaderCB[0] = FXAACB;
				Cmd.Pass = ERenderPass::FXAA;
				Cmd.BuildSortKey(0);
			}
		}
	}

	// --- Font (World → AlphaBlend, Screen → OverlayFont) ---
	{
		const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
		if (FontRes && FontRes->IsLoaded())
		{
			if (FontGeometry.GetWorldQuadCount() > 0 && FontGeometry.UploadWorldBuffers(Ctx))
			{
				FShader* FontShader = FShaderManager::Get().GetShader(EShaderType::Font);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::AlphaBlend);
				Cmd.Shader = FontShader;
				Cmd.Buffer = { FontGeometry.GetWorldVBBuffer(), FontGeometry.GetWorldVBStride(), FontGeometry.GetWorldIBBuffer() };
				Cmd.Buffer.IndexCount = FontGeometry.GetWorldIndexCount();
				Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
				Cmd.BuildSortKey();
			}

			if (FontGeometry.GetScreenQuadCount() > 0 && FontGeometry.UploadScreenBuffers(Ctx))
			{
				FShader* OverlayShader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::OverlayFont);
				Cmd.Shader = OverlayShader;
				Cmd.Buffer = { FontGeometry.GetScreenVBBuffer(), FontGeometry.GetScreenVBStride(), FontGeometry.GetScreenIBBuffer() };
				Cmd.Buffer.IndexCount = FontGeometry.GetScreenIndexCount();
				Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
				Cmd.BuildSortKey();
			}
		}
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

// ============================================================
// PerObjectCB 풀 관리
// ============================================================

void FRenderer::EnsurePerObjectCBPoolCapacity(uint32 RequiredCount)
{
	if (PerObjectCBPool.size() >= RequiredCount)
	{
		return;
	}

	const size_t OldCount = PerObjectCBPool.size();
	PerObjectCBPool.resize(RequiredCount);

	ID3D11Device* D3DDevice = Device.GetDevice();
	for (size_t Index = OldCount; Index < PerObjectCBPool.size(); ++Index)
	{
		PerObjectCBPool[Index].Create(D3DDevice, sizeof(FPerObjectConstants));
	}
}

FConstantBuffer* FRenderer::GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy)
{
	if (Proxy.ProxyId == UINT32_MAX)
	{
		return nullptr;
	}

	EnsurePerObjectCBPoolCapacity(Proxy.ProxyId + 1);
	return &PerObjectCBPool[Proxy.ProxyId];
}


//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame)
{
	FFrameConstants frameConstantData = {};
	frameConstantData.View = Frame.View;
	frameConstantData.Projection = Frame.Proj;
	frameConstantData.InvViewProj = (Frame.View * Frame.Proj).GetInverse();
	frameConstantData.bIsWireframe = (Frame.ViewMode == EViewMode::Wireframe);
	frameConstantData.WireframeColor = Frame.WireframeColor;
	frameConstantData.CameraWorldPos = Frame.CameraPosition;

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}

void FRenderer::UpdateLightBuffer(ID3D11Device* InDevice, ID3D11DeviceContext* Context, const FScene& Scene)
{
	//AmbientLight & DirectionalLight Data Upload
	FLightingCBData GlobalLightingData = {};
	if (Scene.HasGlobalAmbientLight())
	{
		FGlobalAmbientLightParams DirLightParams = Scene.GetGlobalAmbientLightParams();
		GlobalLightingData.Ambient.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Ambient.Color = DirLightParams.LightColor;
	}
	else
	{
		// 폴백: 씬에 AmbientLight 없으면 최소 ambient 보장 (검정 방지)
		GlobalLightingData.Ambient.Intensity = 0.15f;
		GlobalLightingData.Ambient.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	if (Scene.HasGlobalDirectionalLight())
	{
		FGlobalDirectionalLightParams DirLightParams = Scene.GetGlobalDirectionalLightParams();
		GlobalLightingData.Directional.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Directional.Color = DirLightParams.LightColor;
		GlobalLightingData.Directional.Direction = DirLightParams.Direction;
	}
	//else
	//{
	//	// 폴백: 씬에 DirectionalLight 없으면 기본 태양광 (검정 방지)
	//	GlobalLightingData.Directional.Intensity = 1.0f;
	//	GlobalLightingData.Directional.Color = FVector4(1.0f, 0.95f, 0.85f, 1.0f);
	//	GlobalLightingData.Directional.Direction = FVector(1.0f, -1.0f, 0.5f).Normalized();
	//}

	const TArray<FPointLightParams>& PointLightParams = Scene.GetPointLights();
	if (!PointLightParams.empty())
	{
		GlobalLightingData.NumActivePointLights = static_cast<uint32>(PointLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActivePointLights = 0;
	}


	const TArray<FSpotLightParams>& SpotLightParams = Scene.GetSpotLights();
	if (!SpotLightParams.empty())
	{
		GlobalLightingData.NumActiveSpotLights = static_cast<uint32>(SpotLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActiveSpotLights = 0;
	}

	TArray<FLightInfo> Infos;
	for (const FPointLightParams& PointLigth : PointLightParams)
	{
		Infos.emplace_back(PointLigth.ToLightInfo());

	}
	for (const FSpotLightParams& SpotLight : SpotLightParams)
	{
		Infos.emplace_back(SpotLight.ToLightInfo());
	}

	GlobalLightingData.NumTilesX = 0; //똥값. 이후 교체필요
	GlobalLightingData.NumTilesY = 0; //똥값. 이후 교체필요

	Resources.LightingConstantBuffer.Update(Context, &GlobalLightingData, sizeof(FLightingCBData));
	ID3D11Buffer* b4 = Resources.LightingConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);
	Context->PSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);


	Resources.ForwardLights.Update(InDevice, Context, Infos);
	Context->VSSetShaderResources(ELightTexSlot::AllLights, 1, &Resources.ForwardLights.LightBufferSRV);
	Context->PSSetShaderResources(ELightTexSlot::AllLights, 1, &Resources.ForwardLights.LightBufferSRV);
}
