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
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

	static const char* RenderPassNames[] = {
		"RenderPass::Opaque",
		"RenderPass::Font",
		"RenderPass::SubUV",
		"RenderPass::Translucent",
		"RenderPass::SelectionMask",
		"RenderPass::Editor",
		"RenderPass::Grid",
		"RenderPass::PostProcess",
		"RenderPass::GizmoOuter",
		"RenderPass::GizmoInner",
		"RenderPass::OverlayFont",
	};
	static_assert(ARRAYSIZE(RenderPassNames) == (uint32)ERenderPass::MAX, "RenderPassNames must match ERenderPass entries");

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		SCOPE_STAT(RenderPassNames[i]);
		GPU_SCOPE_STAT(RenderPassNames[i]);

		ERenderPass CurPass = static_cast<ERenderPass>(i);
		ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());

		if (PassBatchers[i])
		{
			PassBatchers[i].DrawBatch(CurPass, InRenderBus, Context);
		}
		else
		{
			const auto& Commands = InRenderBus.GetCommands(CurPass);
			if (!Commands.empty())
			{
				ExecuteDefaultPass(Commands, InRenderBus, Context);
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
// 기본 패스 실행기 — 2-pass 링 버퍼 (Write → Unmap → Draw)
// ============================================================
// 기본 패스 실행기 — 중복 상태 바인딩 스킵
void FRenderer::ExecuteDefaultPass(const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	// 이전 프레임/패스 상태를 무효화
	FShader*     LastShader     = nullptr;
	FMeshBuffer* LastMeshBuffer = nullptr;
	bool         bSamplerBound  = false;
	ID3D11ShaderResourceView* LastSRV = reinterpret_cast<ID3D11ShaderResourceView*>(~0ull); // sentinel
	int32        LastUVScroll   = -1; // sentinel: 0 or 1

	for (const auto& Cmd : Commands)
	{
		// --- 셰이더 바인딩 (변경 시에만) ---
		if (Cmd.Shader && Cmd.Shader != LastShader)
		{
			Cmd.Shader->Bind(Context);
			LastShader = Cmd.Shader;
		}

		// --- PerObject CB 업데이트 (매 오브젝트 필수 — 트랜스폼이 다르므로) ---
		Resources.PerObjectConstantBuffer.Update(Context, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
		{
			ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);
		}

		// --- Extra CB (Gizmo, Outline 등 — 있을 때만) ---
		if (Cmd.ExtraCB.Buffer)
		{
			Cmd.ExtraCB.Buffer->Update(Context, Cmd.ExtraCB.Data, Cmd.ExtraCB.Size);
			ID3D11Buffer* cb = Cmd.ExtraCB.Buffer->GetBuffer();
			Context->VSSetConstantBuffers(Cmd.ExtraCB.Slot, 1, &cb);
			Context->PSSetConstantBuffers(Cmd.ExtraCB.Slot, 1, &cb);
		}

		// --- StaticMesh 섹션별 드로우 ---
		if (!Cmd.SectionDraws.empty())
		{
			if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid()) continue;

			// VB/IB 바인딩 (동일 메시면 스킵)
			if (Cmd.MeshBuffer != LastMeshBuffer)
			{
				uint32 offset = 0;
				ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
				if (!vertexBuffer) continue;
				uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
				Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

				ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
				if (!indexBuffer) continue;
				Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				LastMeshBuffer = Cmd.MeshBuffer;
			}

			// Sampler (패스 내 1회만)
			if (!bSamplerBound)
			{
				Context->PSSetSamplers(0, 1, &Resources.DefaultSampler);
				bSamplerBound = true;
			}

			for (const FMeshSectionDraw& Section : Cmd.SectionDraws)
			{
				if (Section.IndexCount == 0) continue;

				// SRV 바인딩 (변경 시에만)
				if (Section.DiffuseSRV != LastSRV)
				{
					ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
					Context->PSSetShaderResources(0, 1, &srv);
					LastSRV = Section.DiffuseSRV;
				}

				// PerObject CB — DiffuseColor 반영
				FPerObjectConstants SectionConstants = Cmd.PerObjectConstants;
				SectionConstants.Color = Section.DiffuseColor;
				Resources.PerObjectConstantBuffer.Update(Context, &SectionConstants, sizeof(FPerObjectConstants));

				// Material CB — UVScroll (변경 시에만)
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
			}
		}
		else
		{
			LastMeshBuffer = nullptr; // DrawCommand가 VB/IB를 바인딩하므로 캐시 무효화
			DrawCommand(Context, Cmd);
		}
	}

	// SRV 언바인딩
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
// 커맨드 바인딩 — 셰이더 + PerObject CB + Extra CB (데이터 드리븐)
// ============================================================
void FRenderer::BindCommand(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
	// 커맨드가 지정한 셰이더 바인딩
	if (InCmd.Shader)
	{
		InCmd.Shader->Bind(Context);
	}

	// 공통 PerObject CB
	Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
	{
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);
	}

	// Extra CB — ExtraCB.Data를 지정 슬롯에 업로드
	if (InCmd.ExtraCB.Buffer)
	{
		InCmd.ExtraCB.Buffer->Update(Context, InCmd.ExtraCB.Data, InCmd.ExtraCB.Size);
		ID3D11Buffer* cb = InCmd.ExtraCB.Buffer->GetBuffer();
		Context->VSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
		Context->PSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
	}
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
	uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
	if (vertexCount == 0 || stride == 0)
	{
		return;
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexCount = InCommand.MeshBuffer->GetIndexBuffer().GetIndexCount();
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, 0, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

void FRenderer::DrawStaticMeshSections(ID3D11DeviceContext* Context, const FRenderCommand& Cmd)
{
	if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid()) return;

	// 버텍스 버퍼 바인딩 (한 번만)
	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (!vertexBuffer) return;
	uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
	Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (!indexBuffer) return;
	Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// StaticMeshShader가 s0에 SamplerState를 요구
	Context->PSSetSamplers(0, 1, &Resources.DefaultSampler);

	for (const FMeshSectionDraw& Section : Cmd.SectionDraws)
	{
		if (Section.IndexCount == 0) continue;

		// 섹션별 SRV 바인딩 (t0)
		ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
		Context->PSSetShaderResources(0, 1, &srv);

		// 섹션별 DiffuseColor를 PrimitiveColor(b1)에 반영
		FPerObjectConstants SectionConstants = Cmd.PerObjectConstants;
		SectionConstants.Color = Section.DiffuseColor;
		Resources.PerObjectConstantBuffer.Update(Context, &SectionConstants, sizeof(FPerObjectConstants));

		// UVScroll (b4) 업데이트
		FConstantBuffer* MaterialCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Material, sizeof(FMaterialConstants));
		FMaterialConstants MatConstants = {};
		MatConstants.bIsUVScroll = Section.bIsUVScroll ? 1 : 0;
		MaterialCB->Update(Context, &MatConstants, sizeof(MatConstants));
		ID3D11Buffer* b4 = MaterialCB->GetBuffer();
		Context->VSSetConstantBuffers(ECBSlot::Material, 1, &b4);

		Context->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
	}

	// SRV 언바인딩 (다음 드로우에 영향 방지)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &nullSRV);
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
	if (Bus.GetCommands(ERenderPass::SelectionMask).empty()) return;

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
