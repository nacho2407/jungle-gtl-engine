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

	InitializePassRenderStates();
	InitializePassBatchers();

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

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);
		const auto& Batcher = PassBatchers[i];
		const bool bHasBatcher = static_cast<bool>(Batcher);
		const bool bHasProxies = !InRenderBus.GetProxies(CurPass).empty();
		if (!bHasBatcher && !bHasProxies) continue;
		if (bHasBatcher && !bHasProxies && Batcher.IsEmpty && Batcher.IsEmpty()) continue;

		const char* PassName = GetRenderPassName(CurPass);
		SCOPE_STAT_CAT(PassName, "4_ExecutePass");
		GPU_SCOPE_STAT(PassName);

		ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());

		if (bHasBatcher)
			PassBatchers[i].DrawBatch(CurPass, InRenderBus, Context);
		else
			ExecutePass(InRenderBus.GetProxies(CurPass), Context);
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
}

// ============================================================
// Pass Batcher DrawBatch 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	PassBatchers[(uint32)ERenderPass::Editor] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			DrawLineBatcher(EditorLineBatcher, Ctx);
		},
		[this]() { return EditorLineBatcher.GetLineCount() == 0; }
	};

	PassBatchers[(uint32)ERenderPass::Grid] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			DrawLineBatcher(GridLineBatcher, Ctx);
		},
		[this]() { return GridLineBatcher.GetLineCount() == 0; }
	};

	PassBatchers[(uint32)ERenderPass::Font] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawBatch(Ctx, FontRes);
		},
		[this]() { return FontBatcher.GetQuadCount() == 0; }
	};

	PassBatchers[(uint32)ERenderPass::OverlayFont] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawScreenBatch(Ctx, FontRes);
		},
		nullptr  // OverlayFont(Stats 등)는 항상 존재
	};

	PassBatchers[(uint32)ERenderPass::SubUV] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.DrawBatch(Ctx);
		},
		[this]() { return SubUVBatcher.GetSpriteCount() == 0; }
	};

	PassBatchers[(uint32)ERenderPass::PostProcess] = {
		[this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			DrawPostProcessOutline(Bus, Ctx);
		},
		nullptr  // PostProcess는 내���에서 SelectionMask 체크
	};
}

// ============================================================
// LineBatcher DrawBatch 공통
// ============================================================
void FRenderer::DrawLineBatcher(FLineBatcher& Batcher, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);
	if (EditorShader) EditorShader->Bind(Context);

	Batcher.DrawBatch(Context);
}

// ============================================================
// 프록시 패스 실행기 — FPrimitiveSceneProxy* 순회
// ============================================================
void FRenderer::ExecutePass(const TArray<const FPrimitiveSceneProxy*>& Proxies, ID3D11DeviceContext* Context)
{
	SortProxies(Proxies);

	FDrawState State;
	BindPerObjectSlot(Context);

	{
		SCOPE_STAT_CAT("ExecutePass::Draw", "4_ExecutePass");
		for (const FPrimitiveSceneProxy* RawProxy : SortedProxyBuffer)
		{
			const FPrimitiveSceneProxy& Proxy = *RawProxy;
			if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid()) continue;
			BindShader(Proxy, Context, State);	
			BindExtraCB(Proxy, Context);
			
			if(Proxy.SectionDraws.size() == 1)
				DrawSingleSection(Proxy, Context, State);
			else if (!Proxy.SectionDraws.empty())
				DrawSections(Proxy, Context, State);
			else
				DrawSimple(Proxy, Context, State);
		}
	}

	CleanupSRV(Context, State);
}

// ============================================================
// ExecutePass 헬퍼 함수들
// ============================================================

void FRenderer::SortProxies(const TArray<const FPrimitiveSceneProxy*>& Proxies)
{
	SCOPE_STAT_CAT("ExecutePass::Sort", "4_ExecutePass");

	const auto ProxyLess = [](const FPrimitiveSceneProxy* A, const FPrimitiveSceneProxy* B)
	{
		if (A->SortKey != B->SortKey)
		{
			return A->SortKey < B->SortKey;
		}

		return A->MaterialSortKey < B->MaterialSortKey;
	};

	// A: capacity 유지 — assign() 대신 clear() + insert()
	SortedProxyBuffer.clear();
	SortedProxyBuffer.insert(SortedProxyBuffer.end(), Proxies.begin(), Proxies.end());

	if (SortedProxyBuffer.size() <= 1) return;

	// B: 이미 정렬되어 있으면 O(N) 체크 후 skip
	bool bAlreadySorted = true;
	for (size_t i = 1; i < SortedProxyBuffer.size(); ++i)
	{
		if (ProxyLess(SortedProxyBuffer[i], SortedProxyBuffer[i - 1]))
		{
			bAlreadySorted = false;
			break;
		}
	}
	if (bAlreadySorted) return;

	// C: Shader/MeshBuffer 뒤에 Material layout까지 정렬해 draw-state grouping을 강화한다.
	std::sort(SortedProxyBuffer.begin(), SortedProxyBuffer.end(), ProxyLess);
}

void FRenderer::BindShader(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State)
{
	if (Proxy.Shader && Proxy.Shader != State.LastShader)
	{
		Proxy.Shader->Bind(Ctx);
		State.LastShader = Proxy.Shader;
	}
}

void FRenderer::BindPerObjectSlot(ID3D11DeviceContext* Ctx)
{
	ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);
}

void FRenderer::BindExtraCB(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx)
{
	if (Proxy.ExtraCB.Buffer)
	{
		Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);
		ID3D11Buffer* cb = Proxy.ExtraCB.Buffer->GetBuffer();
		Ctx->VSSetConstantBuffers(Proxy.ExtraCB.Slot, 1, &cb);
		Ctx->PSSetConstantBuffers(Proxy.ExtraCB.Slot, 1, &cb);
	}
}

bool FRenderer::BindMeshBuffer(FMeshBuffer* Buffer, ID3D11DeviceContext* Ctx, FDrawState& State)
{
	if (Buffer == State.LastMeshBuffer) return true;

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = Buffer->GetVertexBuffer().GetBuffer();
	if (!vertexBuffer) return false;

	uint32 stride = Buffer->GetVertexBuffer().GetStride();
	if (stride == 0) return false;

	Ctx->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = Buffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer)
		Ctx->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	State.LastMeshBuffer = Buffer;
	return true;
}

void FRenderer::DrawSections(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State)
{
	if (!BindMeshBuffer(Proxy.MeshBuffer, Ctx, State)) return;

	// SectionDraw는 IB 필수
	if (!Proxy.MeshBuffer->GetIndexBuffer().GetBuffer()) return;

	if (!State.bSamplerBound)
	{
		Ctx->PSSetSamplers(0, 1, &Resources.DefaultSampler);
		State.bSamplerBound = true;
	}
	// PerObject CB — 프록시당 1회만 업데이트 (Model 행렬)
	Resources.PerObjectConstantBuffer.Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
	
	// Material CB 슬롯 바인딩 (1회)
	FConstantBuffer* MaterialCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Material, sizeof(FMaterialConstants));
	if (!State.bMaterialBound)
	{
		ID3D11Buffer* b4 = MaterialCB->GetBuffer();
		Ctx->VSSetConstantBuffers(ECBSlot::Material, 1, &b4);
		State.bMaterialBound = true;
	}

	for (const FMeshSectionDraw& Section : Proxy.SectionDraws)
	{	
		if (Section.IndexCount == 0) continue;

		// SRV 변경 시에만 바인딩
		if (Section.DiffuseSRV != State.LastSRV)
		{
			ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
			Ctx->PSSetShaderResources(0, 1, &srv);
			State.LastSRV = Section.DiffuseSRV;
		}

		// Material CB — SectionColor 또는 UVScroll 변경 시만 업데이트
		int32 curUVScroll = Section.bIsUVScroll ? 1 : 0;
		if (curUVScroll != State.LastUVScroll
			|| memcmp(&Section.DiffuseColor, &State.LastSectionColor, sizeof(FVector4)) != 0)
		{
			FMaterialConstants MatConstants = {};
			MatConstants.bIsUVScroll = curUVScroll;
			MatConstants.SectionColor = Section.DiffuseColor;
			MaterialCB->Update(Ctx, &MatConstants, sizeof(MatConstants));
			State.LastUVScroll = curUVScroll;
			State.LastSectionColor = Section.DiffuseColor;
		}

		Ctx->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
		FDrawCallStats::Increment();
	}
}

void FRenderer::DrawSingleSection(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State)
{
	const FMeshSectionDraw& Section = Proxy.SectionDraws[0];
	if (Section.IndexCount == 0) return;
	
	if (!BindMeshBuffer(Proxy.MeshBuffer, Ctx, State)) return;
	// SectionDraw는 IB 필수
	if (!Proxy.MeshBuffer->GetIndexBuffer().GetBuffer()) return;

	if (!State.bSamplerBound)
	{
		Ctx->PSSetSamplers(0, 1, &Resources.DefaultSampler);
		State.bSamplerBound = true;
	}
	{
		Resources.PerObjectConstantBuffer.Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
	}

	// Material CB 슬롯 바인딩 (1회)
	FConstantBuffer* MaterialCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Material, sizeof(FMaterialConstants));
	if (!State.bMaterialBound)
	{
		ID3D11Buffer* b4 = MaterialCB->GetBuffer();
		Ctx->VSSetConstantBuffers(ECBSlot::Material, 1, &b4);
		State.bMaterialBound = true;
	}


	// SRV 변경 시에만 바인딩
	if (Section.DiffuseSRV != State.LastSRV)
	{
		ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
		Ctx->PSSetShaderResources(0, 1, &srv);
		State.LastSRV = Section.DiffuseSRV;
	}

	// Material CB — SectionColor 또는 UVScroll 변경 시만 업데이트
	int32 curUVScroll = Section.bIsUVScroll ? 1 : 0;
	if (curUVScroll != State.LastUVScroll
		|| memcmp(&Section.DiffuseColor, &State.LastSectionColor, sizeof(FVector4)) != 0)
	{
		FMaterialConstants MatConstants = {};
		MatConstants.bIsUVScroll = curUVScroll;
		MatConstants.SectionColor = Section.DiffuseColor;
		MaterialCB->Update(Ctx, &MatConstants, sizeof(MatConstants));
		State.LastUVScroll = curUVScroll;
		State.LastSectionColor = Section.DiffuseColor;
	}

	Ctx->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
	FDrawCallStats::Increment();
}

void FRenderer::DrawSimple(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State)
{
	Resources.PerObjectConstantBuffer.Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));

	if (!BindMeshBuffer(Proxy.MeshBuffer, Ctx, State)) return;

	uint32 indexCount = Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount();
	if (indexCount > 0)
		Ctx->DrawIndexed(indexCount, 0, 0);
	else
		Ctx->Draw(Proxy.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
	FDrawCallStats::Increment();
}

void FRenderer::CleanupSRV(ID3D11DeviceContext* Ctx, const FDrawState& State)
{
	if (State.HasBoundSRV())
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		Ctx->PSSetShaderResources(0, 1, &nullSRV);
	}
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
	const FPassRenderState& State = PassRenderStates[(uint32)Pass];

	ERasterizerState Rasterizer = State.Rasterizer;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	Device.SetDepthStencilState(State.DepthStencil);
	Device.SetBlendState(State.Blend);
	Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(State.Topology);
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

	// 4) Outline CB (b3) 업데이트
	FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::PostProcess, sizeof(FOutlinePostProcessConstants));
	FOutlinePostProcessConstants PPConstants;
	PPConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	PPConstants.OutlineThickness = 3.0f;
	OutlineCB->Update(Context, &PPConstants, sizeof(PPConstants));
	ID3D11Buffer* cb = OutlineCB->GetBuffer();
	Context->PSSetConstantBuffers(ECBSlot::PostProcess, 1, &cb);

	// 5) Fullscreen Triangle 드로우 (vertex buffer 없이 SV_VertexID 사용)
	Context->IASetInputLayout(nullptr);
	Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	Context->Draw(3, 0);
	FDrawCallStats::Increment();

	// 6) StencilSRV 언바인딩
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &nullSRV);

	// 7) DSV 재바인딩 (후속 패스에서 뎁스 사용)
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
