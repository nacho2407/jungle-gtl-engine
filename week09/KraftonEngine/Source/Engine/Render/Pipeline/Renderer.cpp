#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"


void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	FShaderManager::Get().Initialize(Device.GetDevice());
	FConstantBufferPool::Get().Initialize(Device.GetDevice());
	Resources.Create(Device.GetDevice());

	EditorLineBatcher.Create(Device.GetDevice());
	GridLineBatcher.Create(Device.GetDevice());
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());
	BillboardBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();
	BillboardBatcher.Release();

	for (FConstantBuffer& CB : PerObjectCBPool)
	{
		CB.Release();
	}
	PerObjectCBPool.clear();

	Resources.Release();
	FConstantBufferPool::Get().Release();
	FShaderManager::Get().Release();
	Device.Release();
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& Bus)
{
	// --- Editor 패스: AABB 디버그 박스 + DebugDraw 라인 → EditorLineBatcher ---
	EditorLineBatcher.Clear();
	for (const auto& Entry : Bus.GetAABBEntries())
	{
		EditorLineBatcher.AddAABB(FBoundingBox{ Entry.AABB.Min, Entry.AABB.Max }, Entry.AABB.Color);
	}
	for (const auto& Entry : Bus.GetDebugLineEntries())
	{
		EditorLineBatcher.AddLine(Entry.Start, Entry.End, Entry.Color.ToVector4());
	}

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	GridLineBatcher.Clear();
	for (const auto& Entry : Bus.GetGridEntries())
	{
		const FVector CameraPos = Bus.GetView().GetInverseFast().GetLocation();
		FVector CameraFwd = Bus.GetCameraRight().Cross(Bus.GetCameraUp());
		CameraFwd.Normalize();

		GridLineBatcher.AddWorldHelpers(
			Bus.GetShowFlags(),
			Entry.Grid.GridSpacing,
			Entry.Grid.GridHalfLineCount,
			CameraPos, CameraFwd, Bus.IsFixedOrtho());
	}

	// --- Font 패스: 월드 공간 텍스트 → FontBatcher ---
	FontBatcher.Clear();
	if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
		FontBatcher.EnsureCharInfoMap(FontRes);
	for (const auto& Entry : Bus.GetFontEntries())
	{
		if (!Entry.Font.Text.empty())
		{
			FontBatcher.AddText(
				Entry.Font.Text,
				Entry.PerObject.Model.GetLocation(),
				Bus.GetCameraRight(),
				Bus.GetCameraUp(),
				Entry.PerObject.Model.GetScale(),
				Entry.Font.Scale
			);
		}
	}

	// --- OverlayFont 패스: 스크린 공간 텍스트 → FontBatcher ---
	FontBatcher.ClearScreen();
	for (const auto& Entry : Bus.GetOverlayFontEntries())
	{
		if (!Entry.Font.Text.empty())
		{
			FontBatcher.AddScreenText(
				Entry.Font.Text,
				Entry.Font.ScreenPosition.X,
				Entry.Font.ScreenPosition.Y,
				Bus.GetViewportWidth(),
				Bus.GetViewportHeight(),
				Entry.Font.Scale
			);
		}
	}

	// --- SubUV 패스: 스프라이트 → SubUVBatcher (Particle SRV 기준 정렬) ---
	SubUVBatcher.Clear();
	{
		const auto& Entries = Bus.GetSubUVEntries();
		SortedSubUVBuffer.clear();
		SortedSubUVBuffer.insert(SortedSubUVBuffer.end(), Entries.begin(), Entries.end());

		if (SortedSubUVBuffer.size() > 1)
		{
			std::sort(SortedSubUVBuffer.begin(), SortedSubUVBuffer.end(),
				[](const FSubUVEntry& A, const FSubUVEntry& B) {
					return A.SubUV.Particle < B.SubUV.Particle;
				});
		}

		for (const auto& Entry : SortedSubUVBuffer)
		{
			if (Entry.SubUV.Particle)
			{
				SubUVBatcher.AddSprite(
					Entry.SubUV.Particle->SRV,
					Entry.PerObject.Model.GetLocation(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Entry.PerObject.Model.GetScale(),
					Entry.SubUV.FrameIndex,
					Entry.SubUV.Particle->Columns,
					Entry.SubUV.Particle->Rows,
					Entry.SubUV.Width,
					Entry.SubUV.Height
				);
			}
		}
	}

	// --- Billboard 패스: 컬러 텍스처 quad → BillboardBatcher (Texture SRV 기준 정렬) ---
	BillboardBatcher.Clear();
	{
		const auto& Entries = Bus.GetBillboardEntries();
		SortedBillboardBuffer.clear();
		SortedBillboardBuffer.insert(SortedBillboardBuffer.end(), Entries.begin(), Entries.end());

		if (SortedBillboardBuffer.size() > 1)
		{
			std::sort(SortedBillboardBuffer.begin(), SortedBillboardBuffer.end(),
				[](const FBillboardEntry& A, const FBillboardEntry& B) {
					return A.Billboard.Texture < B.Billboard.Texture;
				});
		}

		for (const auto& Entry : SortedBillboardBuffer)
		{
			if (Entry.Billboard.Texture)
			{
				BillboardBatcher.AddSprite(
					Entry.Billboard.Texture->SRV,
					Entry.PerObject.Model.GetLocation(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Entry.PerObject.Model.GetScale(),
					Entry.Billboard.Width,
					Entry.Billboard.Height
				);
			}
		}
	}
}

//	스왑체인 백버퍼 복귀 — ImGui 합성 직전에 호출
void FRenderer::BeginFrame()
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
	ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

	Context->ClearRenderTargetView(RTV, Device.GetClearColor());
	Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	const D3D11_VIEWPORT& Viewport = Device.GetViewport();
	Context->RSSetViewports(1, &Viewport);
	Context->OMSetRenderTargets(1, &RTV, DSV);
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	FDrawCallStats::Reset();

	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	{
		SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
		UpdateFrameBuffer(Context, InRenderBus);
	}

	// ProxyQueue → FDrawCommand 변환
	{
		SCOPE_STAT_CAT("BuildDrawCommands", "4_ExecutePass");
		BuildProxyDrawCommands(InRenderBus, Context);
		BuildBatcherDrawCommands(InRenderBus, Context);
	}

	// 커맨드 정렬 (Pass → SortKey 순)
	DrawCommandList.Sort();

	// 정렬된 커맨드를 패스 순서에 따라 제출
	const auto& Cmds = DrawCommandList.GetCommands();
	uint32 CmdIdx = 0;

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		// 이 패스에 해당하는 DrawCommand 범위 찾기
		uint32 PassCmdStart = CmdIdx;
		while (CmdIdx < Cmds.size() && Cmds[CmdIdx].Pass == CurPass)
			++CmdIdx;
		const bool bHasCmds = (CmdIdx > PassCmdStart);

		// PostProcess는 특수 처리 (DSV unbind/rebind 필요)
		if (CurPass == ERenderPass::PostProcess)
		{
			const char* PassName = GetRenderPassName(CurPass);
			SCOPE_STAT_CAT(PassName, "4_ExecutePass");
			GPU_SCOPE_STAT(PassName);
			DrawPostProcessOutline(InRenderBus, Context);
			continue;
		}

		if (!bHasCmds) continue;

		const char* PassName = GetRenderPassName(CurPass);
		SCOPE_STAT_CAT(PassName, "4_ExecutePass");
		GPU_SCOPE_STAT(PassName);

		DrawCommandList.SubmitRange(PassCmdStart, CmdIdx, Device, Context, Resources.DefaultSampler);
	}

	DrawCommandList.Reset();
}

// ============================================================
// ProxyQueue → FDrawCommand 변환
// ============================================================
void FRenderer::BuildProxyDrawCommands(const FRenderBus& InRenderBus, ID3D11DeviceContext* Ctx)
{
	DrawCommandList.Reset();
	EViewMode ViewMode = InRenderBus.GetViewMode();

	// PerObjectCBPool 재할당 방지: 최대 ProxyId를 미리 스캔하여 풀 pre-allocate
	uint32 MaxProxyId = 0;
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		for (const FPrimitiveSceneProxy* Proxy : InRenderBus.GetProxies(static_cast<ERenderPass>(i)))
		{
			if (Proxy && Proxy->ProxyId != UINT32_MAX && Proxy->ProxyId > MaxProxyId)
				MaxProxyId = Proxy->ProxyId;
		}
	}
	EnsurePerObjectCBPoolCapacity(MaxProxyId + 1);

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		const auto& Proxies = InRenderBus.GetProxies(CurPass);
		if (Proxies.empty()) continue;

		const FPassRenderState& PassState = PassRenderStates[i];

		for (const FPrimitiveSceneProxy* Proxy : Proxies)
		{
			if (!Proxy) continue;
			BuildCommandsForProxy(*Proxy, CurPass, PassState, ViewMode, Ctx);
		}
	}
}

void FRenderer::BuildCommandsForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass,
	const FPassRenderState& PassState, EViewMode ViewMode, ID3D11DeviceContext* Ctx)
{
	if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid()) return;

	// Wireframe 모드 처리
	ERasterizerState Rasterizer = PassState.Rasterizer;
	if (PassState.bWireframeAware && ViewMode == EViewMode::Wireframe)
		Rasterizer = ERasterizerState::WireFrame;

	// PerObjectCB 업데이트
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}

	// ExtraCB 업데이트 (Gizmo 등)
	if (Proxy.ExtraCB.Buffer)
	{
		Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);
	}

	// 공유 MaterialCB 가져오기
	FConstantBuffer* MaterialCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Material, sizeof(FMaterialConstants));

	// SectionDraws가 있으면 섹션당 1개 커맨드, 없으면 1개 커맨드
	if (!Proxy.SectionDraws.empty())
	{
		for (const FMeshSectionDraw& Section : Proxy.SectionDraws)
		{
			if (Section.IndexCount == 0) continue;
			// IB 필수
			if (!Proxy.MeshBuffer->GetIndexBuffer().GetBuffer()) continue;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Shader       = Proxy.Shader;
			Cmd.DepthStencil = PassState.DepthStencil;
			Cmd.Blend        = PassState.Blend;
			Cmd.Rasterizer   = Rasterizer;
			Cmd.Topology     = PassState.Topology;
			Cmd.MeshBuffer   = Proxy.MeshBuffer;
			Cmd.FirstIndex   = Section.FirstIndex;
			Cmd.IndexCount   = Section.IndexCount;
			Cmd.PerObjectCB  = PerObjCB;
			Cmd.ExtraCB      = Proxy.ExtraCB.Buffer;
			Cmd.ExtraCBSlot  = Proxy.ExtraCB.Slot;
			Cmd.MaterialCB   = MaterialCB;
			Cmd.DiffuseSRV   = Section.DiffuseSRV;
			Cmd.SectionColor = Section.DiffuseColor;
			Cmd.bIsUVScroll  = Section.bIsUVScroll ? 1u : 0u;
			Cmd.Pass         = Pass;
			Cmd.SortKey      = FDrawCommand::BuildSortKey(Pass, Proxy.Shader, Proxy.MeshBuffer, Section.DiffuseSRV);
		}
	}
	else
	{
		// SectionDraw 없음 — MeshBuffer 전체 드로우
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Shader       = Proxy.Shader;
		Cmd.DepthStencil = PassState.DepthStencil;
		Cmd.Blend        = PassState.Blend;
		Cmd.Rasterizer   = Rasterizer;
		Cmd.Topology     = PassState.Topology;
		Cmd.MeshBuffer   = Proxy.MeshBuffer;
		Cmd.PerObjectCB  = PerObjCB;
		Cmd.ExtraCB      = Proxy.ExtraCB.Buffer;
		Cmd.ExtraCBSlot  = Proxy.ExtraCB.Slot;
		Cmd.Pass         = Pass;
		Cmd.SortKey      = FDrawCommand::BuildSortKey(Pass, Proxy.Shader, Proxy.MeshBuffer, nullptr);
		// IndexCount/VertexCount = 0 → Submit에서 MeshBuffer 전체 드로우
	}
}

// ============================================================
// Batcher → FDrawCommand 변환
// ============================================================
void FRenderer::BuildBatcherDrawCommands(const FRenderBus& InRenderBus, ID3D11DeviceContext* Ctx)
{
	EViewMode ViewMode = InRenderBus.GetViewMode();

	// --- Helper: PassRenderState → FDrawCommand PSO 필드 복사 ---
	auto ApplyPassState = [&](FDrawCommand& Cmd, ERenderPass Pass)
	{
		const FPassRenderState& S = PassRenderStates[(uint32)Pass];
		Cmd.DepthStencil = S.DepthStencil;
		Cmd.Blend        = S.Blend;
		Cmd.Rasterizer   = S.Rasterizer;
		Cmd.Topology     = S.Topology;
		Cmd.Pass         = Pass;

		if (S.bWireframeAware && ViewMode == EViewMode::Wireframe)
			Cmd.Rasterizer = ERasterizerState::WireFrame;
	};

	// --- Editor Line Batcher ---
	if (EditorLineBatcher.GetLineCount() > 0 && EditorLineBatcher.UploadBuffers(Ctx))
	{
		FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::Editor);
		Cmd.Shader      = EditorShader;
		Cmd.RawVB       = EditorLineBatcher.GetVBBuffer();
		Cmd.RawVBStride = EditorLineBatcher.GetVBStride();
		Cmd.RawIB       = EditorLineBatcher.GetIBBuffer();
		Cmd.IndexCount   = EditorLineBatcher.GetIndexCount();
		Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Editor, EditorShader, nullptr, nullptr);
	}

	// --- Grid Line Batcher ---
	if (GridLineBatcher.GetLineCount() > 0 && GridLineBatcher.UploadBuffers(Ctx))
	{
		FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::Grid);
		Cmd.Shader      = EditorShader;
		Cmd.RawVB       = GridLineBatcher.GetVBBuffer();
		Cmd.RawVBStride = GridLineBatcher.GetVBStride();
		Cmd.RawIB       = GridLineBatcher.GetIBBuffer();
		Cmd.IndexCount   = GridLineBatcher.GetIndexCount();
		Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Grid, EditorShader, nullptr, nullptr);
	}

	// --- Font Batcher (World + Screen) ---
	{
		const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
		if (FontRes && FontRes->IsLoaded())
		{
			// World Font
			if (FontBatcher.GetQuadCount() > 0 && FontBatcher.UploadWorldBuffers(Ctx))
			{
				FShader* FontShader = FShaderManager::Get().GetShader(EShaderType::Font);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::Font);
				Cmd.Shader      = FontShader;
				Cmd.RawVB       = FontBatcher.GetWorldVBBuffer();
				Cmd.RawVBStride = FontBatcher.GetWorldVBStride();
				Cmd.RawIB       = FontBatcher.GetWorldIBBuffer();
				Cmd.IndexCount   = FontBatcher.GetWorldIndexCount();
				Cmd.DiffuseSRV   = FontRes->SRV;
				Cmd.Sampler      = FontBatcher.GetSampler();
				Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Font, FontShader, nullptr, FontRes->SRV);
			}

			// Screen / Overlay Font
			if (FontBatcher.GetScreenQuadCount() > 0 && FontBatcher.UploadScreenBuffers(Ctx))
			{
				FShader* OverlayShader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::OverlayFont);
				Cmd.Shader      = OverlayShader;
				Cmd.RawVB       = FontBatcher.GetScreenVBBuffer();
				Cmd.RawVBStride = FontBatcher.GetScreenVBStride();
				Cmd.RawIB       = FontBatcher.GetScreenIBBuffer();
				Cmd.IndexCount   = FontBatcher.GetScreenIndexCount();
				Cmd.DiffuseSRV   = FontRes->SRV;
				Cmd.Sampler      = FontBatcher.GetSampler();
				Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::OverlayFont, OverlayShader, nullptr, FontRes->SRV);
			}
		}
	}

	// --- SRV-batched Batcher 공통 헬퍼 (SubUV, Billboard) ---
	auto EmitSRVBatchCommands = [&](ERenderPass Pass, FShader* Shader,
		ID3D11Buffer* BatchVB, uint32 BatchVBStride, ID3D11Buffer* BatchIB,
		ID3D11SamplerState* BatchSampler, const TArray<FSRVBatch>& Batches)
	{
		for (const FSRVBatch& Batch : Batches)
		{
			if (!Batch.SRV || Batch.IndexCount == 0) continue;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			ApplyPassState(Cmd, Pass);
			Cmd.Shader      = Shader;
			Cmd.RawVB       = BatchVB;
			Cmd.RawVBStride = BatchVBStride;
			Cmd.RawIB       = BatchIB;
			Cmd.FirstIndex   = Batch.IndexStart;
			Cmd.IndexCount   = Batch.IndexCount;
			Cmd.BaseVertex   = Batch.BaseVertex;
			Cmd.DiffuseSRV   = Batch.SRV;
			Cmd.Sampler      = BatchSampler;
			Cmd.SortKey      = FDrawCommand::BuildSortKey(Pass, Shader, nullptr, Batch.SRV);
		}
	};

	// --- SubUV Batcher ---
	if (SubUVBatcher.GetSpriteCount() > 0 && SubUVBatcher.UploadBuffers(Ctx))
	{
		EmitSRVBatchCommands(ERenderPass::SubUV,
			FShaderManager::Get().GetShader(EShaderType::SubUV),
			SubUVBatcher.GetVBBuffer(), SubUVBatcher.GetVBStride(),
			SubUVBatcher.GetIBBuffer(), SubUVBatcher.GetSampler(),
			SubUVBatcher.GetBatches());
	}

	// --- Billboard Batcher ---
	if (BillboardBatcher.GetSpriteCount() > 0 && BillboardBatcher.UploadBuffers(Ctx))
	{
		EmitSRVBatchCommands(ERenderPass::Billboard,
			FShaderManager::Get().GetShader(EShaderType::Billboard),
			BillboardBatcher.GetVBBuffer(), BillboardBatcher.GetVBStride(),
			BillboardBatcher.GetIBBuffer(), BillboardBatcher.GetSampler(),
			BillboardBatcher.GetBatches());
	}
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	//                              DepthStencil                    Blend                Rasterizer                   Topology                                WireframeAware
	S[(uint32)E::Opaque] = { EDepthStencilState::Default,      EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Translucent] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite,  EBlendState::NoColor,    ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::PostProcess] = { EDepthStencilState::NoDepth,       EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Editor] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     true };
	S[(uint32)E::Grid] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     false };
	S[(uint32)E::GizmoOuter] = { EDepthStencilState::GizmoOutside, EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoInner] = { EDepthStencilState::GizmoInside,  EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Font] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::OverlayFont] = { EDepthStencilState::NoDepth,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::SubUV] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Billboard] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
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

// ============================================================
// PostProcess Outline — DSV unbind → StencilSRV bind → Fullscreen Draw
// ============================================================
void FRenderer::DrawPostProcessOutline(const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	ID3D11ShaderResourceView* StencilSRV = Bus.GetViewportStencilSRV();
	ID3D11DepthStencilView* DSV = Bus.GetViewportDSV();
	ID3D11RenderTargetView* RTV = Bus.GetViewportRTV();
	if (!StencilSRV || !RTV) return;

	// SelectionMask 큐가 비어 있으면 선택된 오브젝트 없음 → 스킵
	if (Bus.GetProxies(ERenderPass::SelectionMask).empty()) return;

	// 1) DSV 언바인딩 (StencilSRV와 동시 바인딩 불가)
	Context->OMSetRenderTargets(1, &RTV, nullptr);

	// 2) StencilSRV → PS t0 바인딩
	Context->PSSetShaderResources(0, 1, &StencilSRV);

	// 3) PostProcess 셰이더 바인딩
	FShader* PPShader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
	if (PPShader) PPShader->Bind(Context);

	// 4) PSO 상태 적용
	const FPassRenderState& PPState = PassRenderStates[(uint32)ERenderPass::PostProcess];
	Device.SetDepthStencilState(PPState.DepthStencil);
	Device.SetBlendState(PPState.Blend);
	Device.SetRasterizerState(PPState.Rasterizer);
	Context->IASetPrimitiveTopology(PPState.Topology);

	// 5) Outline CB (b3) 업데이트
	FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::PostProcess, sizeof(FOutlinePostProcessConstants));
	FOutlinePostProcessConstants PPConstants;
	PPConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	PPConstants.OutlineThickness = 3.0f;
	OutlineCB->Update(Context, &PPConstants, sizeof(PPConstants));
	ID3D11Buffer* cb = OutlineCB->GetBuffer();
	Context->PSSetConstantBuffers(ECBSlot::PostProcess, 1, &cb);

	// 6) Fullscreen Triangle 드로우 (vertex buffer 없이 SV_VertexID 사용)
	Context->IASetInputLayout(nullptr);
	Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	Context->Draw(3, 0);
	FDrawCallStats::Increment();

	// 7) StencilSRV 언바인딩
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &nullSRV);

	// 8) DSV 재바인딩 (후속 패스에서 뎁스 사용)
	Context->OMSetRenderTargets(1, &RTV, DSV);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	FFrameConstants frameConstantData = {};
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}
