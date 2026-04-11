#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
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
	Device.Release();
}

//	Bus → dynamic geometry 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& Bus)
{
	// --- Editor 패스: AABB 디버그 박스 + DebugDraw 라인 ---
	EditorLines.Clear();
	for (const auto& AABB : Bus.GetDebugAABBs())
	{
		EditorLines.AddAABB(FBoundingBox{ AABB.Min, AABB.Max }, AABB.Color);
	}
	for (const auto& Line : Bus.GetDebugLines())
	{
		EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
	}

	// --- Grid 패스: 월드 그리드 + 축 ---
	GridLines.Clear();
	if (Bus.HasGrid())
	{
		const FVector CameraPos = Bus.Frame.View.GetInverseFast().GetLocation();
		FVector CameraFwd = Bus.Frame.CameraRight.Cross(Bus.Frame.CameraUp);
		CameraFwd.Normalize();

		GridLines.AddWorldHelpers(
			Bus.Frame.ShowFlags,
			Bus.GetGridSpacing(),
			Bus.GetGridHalfLineCount(),
			CameraPos, CameraFwd, Bus.Frame.IsFixedOrtho());
	}

	// --- Font 패스: 월드 공간 텍스트 (TextRender proxies in Font queue) ---
	FontGeometry.Clear();
	if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
		FontGeometry.EnsureCharInfoMap(FontRes);
	for (const FPrimitiveSceneProxy* Proxy : Bus.GetProxies(ERenderPass::Font))
	{
		if (!Proxy || !Proxy->bVisible) continue;
		const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
		if (TextProxy->CachedText.empty()) continue;
		FontGeometry.AddWorldText(
			TextProxy->CachedText,
			TextProxy->CachedBillboardMatrix.GetLocation(),
			Bus.Frame.CameraRight,
			Bus.Frame.CameraUp,
			TextProxy->CachedBillboardMatrix.GetScale(),
			TextProxy->CachedFontScale
		);
	}

	// --- OverlayFont 패스: 스크린 공간 텍스트 ---
	FontGeometry.ClearScreen();
	for (const auto& Text : Bus.GetOverlayTexts())
	{
		if (!Text.Text.empty())
		{
			FontGeometry.AddScreenText(
				Text.Text,
				Text.Position.X,
				Text.Position.Y,
				Bus.Frame.ViewportWidth,
				Bus.Frame.ViewportHeight,
				Text.Scale
			);
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
		UpdateFrameBuffer(Context, InRenderBus.Frame);
	}

	// ProxyQueue → FDrawCommand 변환
	{
		SCOPE_STAT_CAT("BuildDrawCommands", "4_ExecutePass");
		BuildProxyDrawCommands(InRenderBus, Context);
		BuildDynamicDrawCommands(InRenderBus.Frame, Context);
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
	EViewMode ViewMode = InRenderBus.Frame.ViewMode;

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

		// Font/OverlayFont passes are handled by dynamic commands (FontGeometry)
		if (CurPass == ERenderPass::Font || CurPass == ERenderPass::OverlayFont)
			continue;

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

	// ExtraCB 업데이트 (Gizmo, SubUV 등) — lazy creation if buffer not yet allocated
	if (Proxy.ExtraCB.Buffer)
	{
		if (!Proxy.ExtraCB.Buffer->GetBuffer())
			Proxy.ExtraCB.Buffer->Create(Device.GetDevice(), Proxy.ExtraCB.Size);
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
		Cmd.DiffuseSRV   = Proxy.DiffuseSRV;
		Cmd.Sampler      = Proxy.Sampler;
		Cmd.Pass         = Pass;
		Cmd.SortKey      = FDrawCommand::BuildSortKey(Pass, Proxy.Shader, Proxy.MeshBuffer, Proxy.DiffuseSRV);
		// IndexCount/VertexCount = 0 → Submit에서 MeshBuffer 전체 드로우
	}
}

// ============================================================
// Dynamic geometry → FDrawCommand 변환 (Font, Line)
// ============================================================
void FRenderer::BuildDynamicDrawCommands(const FFrameContext& Frame, ID3D11DeviceContext* Ctx)
{
	EViewMode ViewMode = Frame.ViewMode;

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

	// --- Editor Lines ---
	if (EditorLines.GetLineCount() > 0 && EditorLines.UploadBuffers(Ctx))
	{
		FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::Editor);
		Cmd.Shader      = EditorShader;
		Cmd.RawVB       = EditorLines.GetVBBuffer();
		Cmd.RawVBStride = EditorLines.GetVBStride();
		Cmd.RawIB       = EditorLines.GetIBBuffer();
		Cmd.IndexCount   = EditorLines.GetIndexCount();
		Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Editor, EditorShader, nullptr, nullptr);
	}

	// --- Grid Lines ---
	if (GridLines.GetLineCount() > 0 && GridLines.UploadBuffers(Ctx))
	{
		FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		ApplyPassState(Cmd, ERenderPass::Grid);
		Cmd.Shader      = EditorShader;
		Cmd.RawVB       = GridLines.GetVBBuffer();
		Cmd.RawVBStride = GridLines.GetVBStride();
		Cmd.RawIB       = GridLines.GetIBBuffer();
		Cmd.IndexCount   = GridLines.GetIndexCount();
		Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Grid, EditorShader, nullptr, nullptr);
	}

	// --- Font (World + Screen) ---
	{
		const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
		if (FontRes && FontRes->IsLoaded())
		{
			// World Font
			if (FontGeometry.GetWorldQuadCount() > 0 && FontGeometry.UploadWorldBuffers(Ctx))
			{
				FShader* FontShader = FShaderManager::Get().GetShader(EShaderType::Font);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::Font);
				Cmd.Shader      = FontShader;
				Cmd.RawVB       = FontGeometry.GetWorldVBBuffer();
				Cmd.RawVBStride = FontGeometry.GetWorldVBStride();
				Cmd.RawIB       = FontGeometry.GetWorldIBBuffer();
				Cmd.IndexCount   = FontGeometry.GetWorldIndexCount();
				Cmd.DiffuseSRV   = FontRes->SRV;
				Cmd.Sampler      = FontGeometry.GetSampler();
				Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::Font, FontShader, nullptr, FontRes->SRV);
			}

			// Screen / Overlay Font
			if (FontGeometry.GetScreenQuadCount() > 0 && FontGeometry.UploadScreenBuffers(Ctx))
			{
				FShader* OverlayShader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);

				FDrawCommand& Cmd = DrawCommandList.AddCommand();
				ApplyPassState(Cmd, ERenderPass::OverlayFont);
				Cmd.Shader      = OverlayShader;
				Cmd.RawVB       = FontGeometry.GetScreenVBBuffer();
				Cmd.RawVBStride = FontGeometry.GetScreenVBStride();
				Cmd.RawIB       = FontGeometry.GetScreenIBBuffer();
				Cmd.IndexCount   = FontGeometry.GetScreenIndexCount();
				Cmd.DiffuseSRV   = FontRes->SRV;
				Cmd.Sampler      = FontGeometry.GetSampler();
				Cmd.SortKey      = FDrawCommand::BuildSortKey(ERenderPass::OverlayFont, OverlayShader, nullptr, FontRes->SRV);
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
	ID3D11ShaderResourceView* StencilSRV = Bus.Frame.ViewportStencilSRV;
	ID3D11DepthStencilView* DSV = Bus.Frame.ViewportDSV;
	ID3D11RenderTargetView* RTV = Bus.Frame.ViewportRTV;
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

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame)
{
	FFrameConstants frameConstantData = {};
	frameConstantData.View = Frame.View;
	frameConstantData.Projection = Frame.Proj;
	frameConstantData.bIsWireframe = (Frame.ViewMode == EViewMode::Wireframe);
	frameConstantData.WireframeColor = Frame.WireframeColor;

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}
