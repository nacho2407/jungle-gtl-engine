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
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	EditorLineBatcher.Clear();
	for (const auto& Entry : Bus.GetAABBEntries())
	{
		EditorLineBatcher.AddAABB(FBoundingBox{ Entry.AABB.Min, Entry.AABB.Max }, Entry.AABB.Color);
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
		SortedSubUVBuffer.assign(Entries.begin(), Entries.end());

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
	UpdateFrameBuffer(Context, InRenderBus);

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);
		const bool bHasBatcher = static_cast<bool>(PassBatchers[i]);
		const bool bHasProxies = !InRenderBus.GetProxies(CurPass).empty();
		if (!bHasBatcher && !bHasProxies) continue;

		const char* PassName = GetRenderPassName(CurPass);
		SCOPE_STAT(PassName);
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
		}
	};

	PassBatchers[(uint32)ERenderPass::Grid] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			DrawLineBatcher(GridLineBatcher, Ctx);
		}
	};

	PassBatchers[(uint32)ERenderPass::Font] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawBatch(Ctx, FontRes);
		}
	};

	PassBatchers[(uint32)ERenderPass::OverlayFont] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawScreenBatch(Ctx, FontRes);
		}
	};

	PassBatchers[(uint32)ERenderPass::SubUV] = {
		[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.DrawBatch(Ctx);
		}
	};

	PassBatchers[(uint32)ERenderPass::PostProcess] = {
		[this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			DrawPostProcessOutline(Bus, Ctx);
		}
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
	// Shader → MeshBuffer 기준 정렬 (state change 최소화)
	SortedProxyBuffer.assign(Proxies.begin(), Proxies.end());
	if (SortedProxyBuffer.size() > 1)
	{
		std::sort(SortedProxyBuffer.begin(), SortedProxyBuffer.end(),
			[](const FPrimitiveSceneProxy* A, const FPrimitiveSceneProxy* B)
			{
				if (A->Shader != B->Shader)
					return A->Shader < B->Shader;
				return A->MeshBuffer < B->MeshBuffer;
			});
	}

	FShader*     LastShader     = nullptr;
	FMeshBuffer* LastMeshBuffer = nullptr;
	bool         bSamplerBound  = false;
	bool         bPerObjectBound = false;
	ID3D11ShaderResourceView* LastSRV = reinterpret_cast<ID3D11ShaderResourceView*>(~0ull);
	int32        LastUVScroll   = -1;

	for (const FPrimitiveSceneProxy* RawItem : SortedProxyBuffer)
	{
		const FPrimitiveSceneProxy& Item = *RawItem;

		if (!Item.MeshBuffer || !Item.MeshBuffer->IsValid()) continue;

		// --- 셰이더 바인딩 (변경 시에만) ---
		if (Item.Shader && Item.Shader != LastShader)
		{
			Item.Shader->Bind(Context);
			LastShader = Item.Shader;
		}

		// --- PerObject CB 슬롯 바인딩 (1회만) ---
		if (!bPerObjectBound)
		{
			ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);
			bPerObjectBound = true;
		}

		// --- Extra CB (Gizmo 등 — 있을 때만) ---
		if (Item.ExtraCB.Buffer)
		{
			Item.ExtraCB.Buffer->Update(Context, Item.ExtraCB.Data, Item.ExtraCB.Size);
			ID3D11Buffer* cb = Item.ExtraCB.Buffer->GetBuffer();
			Context->VSSetConstantBuffers(Item.ExtraCB.Slot, 1, &cb);
			Context->PSSetConstantBuffers(Item.ExtraCB.Slot, 1, &cb);
		}

		// --- 섹션별 드로우 (StaticMesh) ---
		if (!Item.SectionDraws.empty())
		{
			// VB/IB 바인딩 (동일 메시면 스킵)
			if (Item.MeshBuffer != LastMeshBuffer)
			{
				uint32 offset = 0;
				ID3D11Buffer* vertexBuffer = Item.MeshBuffer->GetVertexBuffer().GetBuffer();
				if (!vertexBuffer) continue;
				uint32 stride = Item.MeshBuffer->GetVertexBuffer().GetStride();
				Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

				ID3D11Buffer* indexBuffer = Item.MeshBuffer->GetIndexBuffer().GetBuffer();
				if (!indexBuffer) continue;
				Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				LastMeshBuffer = Item.MeshBuffer;
			}

			if (!bSamplerBound)
			{
				Context->PSSetSamplers(0, 1, &Resources.DefaultSampler);
				bSamplerBound = true;
			}

			for (const FMeshSectionDraw& Section : Item.SectionDraws)
			{
				if (Section.IndexCount == 0) continue;

				if (Section.DiffuseSRV != LastSRV)
				{
					ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
					Context->PSSetShaderResources(0, 1, &srv);
					LastSRV = Section.DiffuseSRV;
				}

				// Section Color만 오버라이드 — PerObject CB 업데이트
				FPerObjectConstants SectionConstants = Item.PerObjectConstants;
				SectionConstants.Color = Section.DiffuseColor;
				Resources.PerObjectConstantBuffer.Update(Context, &SectionConstants, sizeof(FPerObjectConstants));

				int32 curUVScroll = Section.bIsUVScroll ? 1 : 0;
				if (curUVScroll != LastUVScroll)
				{
					FConstantBuffer* MaterialCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Material, sizeof(FMaterialConstants));
					FMaterialConstants MatConstants = {};
					MatConstants.bIsUVScroll = curUVScroll;
					MaterialCB->Update(Context, &MatConstants, sizeof(MatConstants));
					ID3D11Buffer* b4 = MaterialCB->GetBuffer();
					Context->VSSetConstantBuffers(ECBSlot::Material, 1, &b4);
					LastUVScroll = curUVScroll;
				}

				Context->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
				FDrawCallStats::Increment();
			}
		}
		else
		{
			// 비-섹션 메시 (기본 Primitive) — PerObject CB 업데이트 + VB/IB 바인딩 + Draw
			Resources.PerObjectConstantBuffer.Update(Context, &Item.PerObjectConstants, sizeof(FPerObjectConstants));

			// VB/IB 바인딩 (동일 메시면 스킵)
			if (Item.MeshBuffer != LastMeshBuffer)
			{
				uint32 offset = 0;
				ID3D11Buffer* vertexBuffer = Item.MeshBuffer->GetVertexBuffer().GetBuffer();
				if (!vertexBuffer) continue;

				uint32 vertexCount = Item.MeshBuffer->GetVertexBuffer().GetVertexCount();
				uint32 stride = Item.MeshBuffer->GetVertexBuffer().GetStride();
				if (vertexCount == 0 || stride == 0) continue;

				Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

				ID3D11Buffer* indexBuffer = Item.MeshBuffer->GetIndexBuffer().GetBuffer();
				if (indexBuffer)
					Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				LastMeshBuffer = Item.MeshBuffer;
			}

			uint32 indexCount = Item.MeshBuffer->GetIndexBuffer().GetIndexCount();
			if (indexCount > 0)
				Context->DrawIndexed(indexCount, 0, 0);
			else
				Context->Draw(Item.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
			FDrawCallStats::Increment();
		}
	}

	if (LastSRV != reinterpret_cast<ID3D11ShaderResourceView*>(~0ull))
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		Context->PSSetShaderResources(0, 1, &nullSRV);
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
