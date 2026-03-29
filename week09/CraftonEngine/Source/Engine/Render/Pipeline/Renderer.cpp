#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Core/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"


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
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		if (!PassBatchers[i]) continue;

		const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
		const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

		PassBatchers[i].Clear();
		for (const auto& Cmd : AlignedCommands)
			PassBatchers[i].Collect(Cmd, InRenderBus);
	}
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands)
{
	// SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
	if (Pass == ERenderPass::SubUV && Commands.size() > 1)
	{
		SortedCommandBuffer.assign(Commands.begin(), Commands.end());

		std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
			[](const FRenderCommand& A, const FRenderCommand& B) {
				return A.Params.SubUV.Particle < B.Params.SubUV.Particle;
			});

		return SortedCommandBuffer;
	}

	return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
	ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

	Context->ClearRenderTargetView(RTV, Device.GetClearColor());
	Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const D3D11_VIEWPORT& Viewport = Device.GetViewport();
	Context->RSSetViewports(1, &Viewport);

	Device.SetRasterizerState(ERasterizerState::SolidBackCull);
	Device.SetDepthStencilState(EDepthStencilState::Default);
	Device.SetBlendState(EBlendState::Opaque);

	Context->OMSetRenderTargets(1, &RTV, DSV);

#if STATS
	FGPUProfiler::Get().BeginFrame();
#endif
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);
		const auto& Commands = InRenderBus.GetCommands(CurPass);
		if (Commands.empty()) continue;

		if (PassBatchers[i])
		{
			ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());
			PassBatchers[i].Flush(CurPass, InRenderBus, Context);
		}
		else
		{
			ExecuteDefaultPass(CurPass, Commands, InRenderBus, Context);
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
	S[(uint32)E::Opaque]      = { EDepthStencilState::Default,      EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true  };
	S[(uint32)E::Translucent] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::StencilMask] = { EDepthStencilState::StencilWrite,  EBlendState::Opaque,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Outline]     = { EDepthStencilState::StencilOutline,EBlendState::Opaque,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Editor]      = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     true  };
	S[(uint32)E::Grid]        = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     false };
	S[(uint32)E::DepthLess]   = { EDepthStencilState::DepthReadOnly,EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Font]        = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true  };
	S[(uint32)E::OverlayFont] = { EDepthStencilState::NoDepth,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true  };
	S[(uint32)E::SubUV]       = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true  };
}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Editor] = {
		/*.Clear   =*/ [this]() { EditorLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus&) {
			if (Cmd.Type == ERenderCommandType::DebugBox)
			{
				EditorLineBatcher.AddAABB(FBoundingBox{ Cmd.Params.AABB.Min, Cmd.Params.AABB.Max }, Cmd.Params.AABB.Color);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Grid] = {
		/*.Clear   =*/ [this]() { GridLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Grid)
			{
				const FVector CameraPos = Bus.GetView().GetInverseFast().GetLocation();
				FVector CameraFwd = Bus.GetCameraRight().Cross(Bus.GetCameraUp());
				CameraFwd.Normalize();

				GridLineBatcher.AddWorldHelpers(
					Bus.GetShowFlags(),
					Cmd.Params.Grid.GridSpacing,
					Cmd.Params.Grid.GridHalfLineCount,
					CameraPos, CameraFwd);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(GridLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Font 패스: 텍스트 → FontBatcher ---
	PassBatchers[(uint32)ERenderPass::Font] = {
		/*.Clear   =*/ [this]() { FontBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Params.Font.Text && !Cmd.Params.Font.Text->empty() && !Cmd.Params.Font.bScreenSpace)
			{
				FontBatcher.AddText(
					*Cmd.Params.Font.Text,
					Cmd.PerObjectConstants.Model.GetLocation(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScale(),
					Cmd.Params.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.Flush(Ctx, FontRes);
		}
	};

	// --- OverlayFont 패스: 스크린 텍스트 → FontBatcher ---
	PassBatchers[(uint32)ERenderPass::OverlayFont] = {
		/*.Clear   =*/ [this]() {FontBatcher.ClearScreen(); }, // Font 시작 시에 클리어
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Params.Font.Text && !Cmd.Params.Font.Text->empty() && Cmd.Params.Font.bScreenSpace)
			{
				FontBatcher.AddScreenText(
					*Cmd.Params.Font.Text,
					Cmd.Params.Font.ScreenPosition.X,
					Cmd.Params.Font.ScreenPosition.Y,
					Bus.GetViewportWidth(),
					Bus.GetViewportHeight(),
					Cmd.Params.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.FlushScreen(Ctx, FontRes);
		}
	};

	// --- SubUV 패스: 스프라이트 → SubUVBatcher ---
	PassBatchers[(uint32)ERenderPass::SubUV] = {
		/*.Clear   =*/ [this]() {
			SubUVBatcher.Clear();
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Params.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Params.SubUV;
				SubUVBatcher.AddSprite(
					SubUV.Particle->SRV,
					Cmd.PerObjectConstants.Model.GetLocation(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScale(),
					SubUV.FrameIndex,
					SubUV.Particle->Columns,
					SubUV.Particle->Rows,
					SubUV.Width,
					SubUV.Height
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.Flush(Ctx);
		}
	};
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	const FVector CameraPosition = Bus.GetView().GetInverseFast().GetLocation();
	FEditorConstants EditorConstants = {};
	EditorConstants.CameraPosition = CameraPosition;

	FConstantBuffer* EditorCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Editor, sizeof(FEditorConstants));
	EditorCB->Update(Context, &EditorConstants, sizeof(FEditorConstants));

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);
	if (EditorShader) EditorShader->Bind(Context);

	ID3D11Buffer* cb = EditorCB->GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Editor, 1, &cb);
	Context->PSSetConstantBuffers(ECBSlot::Editor, 1, &cb);

	Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];
	for (const auto& Cmd : Commands)
	{
		EDepthStencilState TargetDepth = (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1))
			? Cmd.DepthStencilState
			: State.DepthStencil;

		EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1))
			? Cmd.BlendState
			: State.Blend;

		Device.SetDepthStencilState(TargetDepth);
		Device.SetBlendState(TargetBlend);

		BindCommand(Cmd, Context);

		// StaticMesh: 섹션별 SRV 바인딩 + 분할 드로우
		if (Cmd.Type == ERenderCommandType::StaticMesh && !Cmd.SectionDraws.empty())
		{
			DrawStaticMeshSections(Context, Cmd);
		}
		else
		{
			DrawCommand(Context, Cmd);
		}
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

	// Extra CB — Params 데이터를 지정 슬롯에 업로드
	if (InCmd.ExtraCB.Buffer)
	{
		InCmd.ExtraCB.Buffer->Update(Context, &InCmd.Params, InCmd.ExtraCB.Size);
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

		Context->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
	}

	// SRV 언바인딩 (다음 드로우에 영향 방지)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &nullSRV);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}
