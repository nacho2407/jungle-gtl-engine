#include "Renderer.h"
#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	// 1. 일반 메쉬 (Primitive.hlsl)
	Resources.PrimitiveShader.Create(Device.GetDevice(), L"Shaders/Primitive.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 2. 기즈모 (Gizmo.hlsl)
	Resources.GizmoShader.Create(Device.GetDevice(), L"Shaders/Gizmo.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 3. 에디터/라인 (Editor.hlsl)
	Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 5. 아웃라인 (Outline.hlsl)
	Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/Outline.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 6. 스태틱 메시 (ShaderStaticMesh.hlsl)
	Resources.StaticMeshShader.Create(Device.GetDevice(), L"Shaders/ShaderStaticMesh.hlsl",
		"mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
	Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
	Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
	Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));
	Resources.StaticMeshConstantBuffer.Create(Device.GetDevice(), sizeof(FStaticMeshConstants));

	// TODO : SamplerState 관리
	D3D11_SAMPLER_DESC SampDesc = {};
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	Device.GetDevice()->CreateSamplerState(&SampDesc, &Resources.MeshSamplerState);


	//	MeshManager init
	FMeshManager::Initialize();

	EditorLineBatcher.Create(Device.GetDevice());
	GridLineBatcher.Create(Device.GetDevice());

	// 텍스처는 ResourceManager가 소유 — Batcher는 셰이더/버퍼만 초기화
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();
	InitializePassBatchers();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	Resources.PrimitiveShader.Release();
	Resources.GizmoShader.Release();
	Resources.EditorShader.Release();
	Resources.OutlineShader.Release();
	Resources.StaticMeshShader.Release();

	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
	Resources.GizmoPerObjectConstantBuffer.Release();
	Resources.EditorConstantBuffer.Release();
	Resources.OutlineConstantBuffer.Release();
	Resources.StaticMeshConstantBuffer.Release();
	Resources.MeshSamplerState->Release();

	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

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
				return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle;
			});

		return SortedCommandBuffer;
	}

	return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();

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

	//                              DepthStencil                   Blend                Rasterizer                  Topology                                Shader                   WireframeAware
	S[(uint32)E::Opaque] = { EDepthStencilState::Default,      EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader, true };
	S[(uint32)E::Translucent] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader, false };
	S[(uint32)E::StencilMask] = { EDepthStencilState::StencilWrite,  EBlendState::Opaque,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader, false };
	S[(uint32)E::Outline] = { EDepthStencilState::StencilOutline,EBlendState::Opaque,    ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.OutlineShader,   false };
	S[(uint32)E::Editor] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,    true };
	S[(uint32)E::Grid] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,    false };
	S[(uint32)E::DepthLess] = { EDepthStencilState::DepthReadOnly,EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.GizmoShader,     false };
	S[(uint32)E::Font] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                    true };
	S[(uint32)E::SubUV] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                    true };
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
				EditorLineBatcher.AddAABB(FBoundingBox{ Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max }, Cmd.Constants.AABB.Color);
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
				const FVector CameraPos = Bus.GetView().GetInverse().GetOrigin();
				FVector CameraFwd = Bus.GetCameraRight().CrossProduct(Bus.GetCameraUp());
				CameraFwd.Normalize();

				GridLineBatcher.AddWorldHelpers(
					Bus.GetShowFlags(),
					Cmd.Constants.Grid.GridSpacing,
					Cmd.Constants.Grid.GridHalfLineCount,
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
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text && !Cmd.Constants.Font.Text->empty())
			{
				FontBatcher.AddText(
					*Cmd.Constants.Font.Text,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					Cmd.Constants.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.Flush(Ctx, FontRes);
		}
	};

	// --- SubUV 패스: 스프라이트 → SubUVBatcher ---
	// Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
	PassBatchers[(uint32)ERenderPass::SubUV] = {
		/*.Clear   =*/ [this]() {
			SubUVBatcher.Clear();
			SubUVCachedSRV = nullptr;
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Constants.SubUV;
				if (!SubUVCachedSRV && SubUV.Particle->IsLoaded())
				{
					SubUVCachedSRV = SubUV.Particle->SRV;
				}

				SubUVBatcher.AddSprite(
					SubUV.Particle->SRV,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
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

	const FVector CameraPosition = Bus.GetView().GetInverse().GetOrigin();
	FEditorConstants EditorConstants = {};
	EditorConstants.CameraPosition = CameraPosition;
	Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &cb);
	Context->PSSetConstantBuffers(4, 1, &cb);

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

		BindShaderByType(Cmd, Context);
		DrawCommand(Context, Cmd);
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

	if (State.Shader)
	{
		State.Shader->Bind(Context);
	}
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
	Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
	{
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(1, 1, &cb);
	}

	switch (InCmd.Type)
	{
	case ERenderCommandType::Gizmo:
		Resources.GizmoShader.Bind(Context);
		Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo, sizeof(FGizmoConstants));
		{
			ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
			cb = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(2, 1, &cb);
			Context->PSSetConstantBuffers(2, 1, &cb);
		}
		break;

	case ERenderCommandType::DebugBox:
		Resources.EditorConstantBuffer.Update(Context, &InCmd.Constants.Editor, sizeof(FEditorConstants));
		{
			ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(4, 1, &cb);
			Context->PSSetConstantBuffers(4, 1, &cb);
			cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
			Context->PSSetConstantBuffers(1, 1, &cb);
		}
		break;

	case ERenderCommandType::SelectionOutline:
		Resources.OutlineConstantBuffer.Update(Context, &InCmd.Constants.Outline, sizeof(FOutlineConstants));
		{
			ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(5, 1, &cb);
			Context->PSSetConstantBuffers(5, 1, &cb);
			cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
		}
		break;

	case ERenderCommandType::StaticMesh:
		Resources.StaticMeshShader.Bind(Context);
		Resources.StaticMeshConstantBuffer.Update(Context, &InCmd.Constants.StaticMesh, sizeof(FStaticMeshConstants));
		{
			ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
			Context->PSSetConstantBuffers(1, 1, &cb);

			cb = Resources.StaticMeshConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(6, 1, &cb);
			Context->PSSetConstantBuffers(6, 1, &cb);

			ID3D11ShaderResourceView* SRVs[4] = {
				InCmd.Constants.StaticMesh.DiffuseSRV,
				InCmd.Constants.StaticMesh.AmbientSRV,
				InCmd.Constants.StaticMesh.SpecularSRV,
				InCmd.Constants.StaticMesh.BumpSRV
			};
			Context->PSSetShaderResources(0, 4, SRVs);
			Context->PSSetSamplers(0, 1, &Resources.MeshSamplerState);

		}
		break;
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
		uint32 indexStart = InCommand.SectionIndexStart;
		uint32 indexCount = InCommand.SectionIndexCount;
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, indexStart, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Device.EndFrame();
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
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}
