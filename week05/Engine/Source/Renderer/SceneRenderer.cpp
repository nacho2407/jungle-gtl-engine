#include "Renderer/SceneRenderer.h"

#include <algorithm>

#include "Renderer/Material.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"

void FSceneRenderer::BeginFrame()
{
	ClearCommandList();
}

size_t FSceneRenderer::GetPrevCommandCount() const
{
	return PrevCommandCount;
}

void FSceneRenderer::BuildQueue(
	FRenderer& Renderer,
	const FSceneRenderPacket& Packet,
	const FSceneViewRenderRequest& SceneView,
	FRenderCommandQueue& OutQueue)
{
	OutQueue.Clear();
	OutQueue.Reserve(Renderer.GetPrevCommandCount());
	OutQueue.ViewMatrix = SceneView.ViewMatrix;
	OutQueue.ProjectionMatrix = SceneView.ProjectionMatrix;

	FSceneCommandBuildContext BuildContext;
	BuildContext.DefaultMaterial = Renderer.GetDefaultMaterial();
	BuildContext.TextFeature = Renderer.GetSceneTextFeature();
	BuildContext.SubUVFeature = Renderer.GetSceneSubUVFeature();
	BuildContext.TotalTimeSeconds = SceneView.TotalTimeSeconds;

	SceneCommandBuilder.BuildQueue(BuildContext, Packet, SceneView.CameraPosition, OutQueue);
}

void FSceneRenderer::AddCommand(FRenderer& Renderer, const FRenderCommand& Command)
{
	CommandList.push_back(Command);
	FRenderCommand& Added = CommandList.back();
	if (!Added.Material)
	{
		Added.Material = Renderer.GetDefaultMaterial();
	}

	Added.SortKey = FRenderCommand::MakeSortKey(Added.Material, Added.RenderMesh);
	Added.SubmissionOrder = NextSubmissionOrder++;
}

void FSceneRenderer::ClearCommandList()
{
	PrevCommandCount = CommandList.size();
	CommandList.clear();
	CommandList.reserve(PrevCommandCount);
	NextSubmissionOrder = 0;
}

void FSceneRenderer::SubmitCommands(FRenderer& Renderer, const FRenderCommandQueue& Queue)
{
	ID3D11Device* Device = Renderer.GetDevice();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!Device || !DeviceContext)
	{
		return;
	}

	Renderer.ViewMatrix = Queue.ViewMatrix;
	Renderer.ProjectionMatrix = Queue.ProjectionMatrix;

	for (const FRenderCommand& Command : Queue.Commands)
	{
		if (Command.RenderMesh)
		{
			Command.RenderMesh->UpdateVertexAndIndexBuffer(Device, DeviceContext);
		}

		AddCommand(Renderer, Command);
	}
}

void FSceneRenderer::ExecuteCommands(FRenderer& Renderer)
{
	std::sort(
		CommandList.begin(),
		CommandList.end(),
		[](const FRenderCommand& A, const FRenderCommand& B)
		{
			if (A.RenderLayer != B.RenderLayer)
			{
				return A.RenderLayer < B.RenderLayer;
			}

			if (A.RenderLayer == ERenderLayer::UI)
			{
				return A.SubmissionOrder < B.SubmissionOrder;
			}

			return A.SortKey < B.SortKey;
		});

	Renderer.SetConstantBuffers();
	Renderer.UpdateFrameConstantBuffer();

	ExecuteRenderPass(Renderer, ERenderLayer::Default);
	Renderer.ClearDepthBuffer();
	ExecuteRenderPass(Renderer, ERenderLayer::Overlay);
	ExecuteRenderPass(Renderer, ERenderLayer::UI);
	ClearCommandList();
}

void FSceneRenderer::ExecuteRenderPass(FRenderer& Renderer, ERenderLayer RenderLayer)
{
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	FMaterial* CurrentMaterial = nullptr;
	FRenderMesh* CurrentMesh = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	FRenderCommand Sentinel;
	Sentinel.RenderLayer = RenderLayer;
	auto It = std::lower_bound(
		CommandList.begin(),
		CommandList.end(),
		Sentinel,
		[](const FRenderCommand& A, const FRenderCommand& B)
		{
			return A.RenderLayer < B.RenderLayer;
		});

	Renderer.GetRenderStateManager()->RebindState();
	for (; It != CommandList.end(); ++It)
	{
		const FRenderCommand& Command = *It;
		if (Command.RenderLayer != RenderLayer)
		{
			return;
		}

		if (!Command.RenderMesh)
		{
			continue;
		}

		if (Command.Material != CurrentMaterial)
		{
			Command.Material->Bind(DeviceContext);

			Renderer.GetRenderStateManager()->BindState(Command.Material->GetRasterizerState());
			Renderer.GetRenderStateManager()->BindState(Command.Material->GetDepthStencilState());
			Renderer.GetRenderStateManager()->BindState(Command.Material->GetBlendState());

			CurrentMaterial = Command.Material;

			if (!CurrentMaterial->HasPixelTextureBinding())
			{
				DeviceContext->PSSetSamplers(0, 1, &Renderer.NormalSampler);
			}
		}

		if (Command.Material)
		{
			if (Command.bDisableCulling)
			{
				FRasterizerStateOption RasterOpt = Command.Material->GetRasterizerOption();
				RasterOpt.CullMode = D3D11_CULL_NONE;
				auto OverrideRS = Renderer.GetRenderStateManager()->GetOrCreateRasterizerState(RasterOpt);
				Renderer.GetRenderStateManager()->BindState(OverrideRS);
			}
			else
			{
				Renderer.GetRenderStateManager()->BindState(Command.Material->GetRasterizerState());
			}

			if (Command.bDisableDepthTest || Command.bDisableDepthWrite)
			{
				FDepthStencilStateOption DepthOpt = Command.Material->GetDepthStencilOption();
				if (Command.bDisableDepthTest)
				{
					DepthOpt.DepthEnable = false;
				}
				if (Command.bDisableDepthWrite)
				{
					DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				}

				auto OverrideDSS = Renderer.GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOpt);
				Renderer.GetRenderStateManager()->BindState(OverrideDSS);
			}
			else
			{
				Renderer.GetRenderStateManager()->BindState(Command.Material->GetDepthStencilState());
			}
		}

		if (Command.RenderMesh->Vertices.empty() && Command.RenderMesh->Indices.empty())
		{
			continue;
		}

		if (Command.RenderMesh != CurrentMesh)
		{
			Command.RenderMesh->Bind(DeviceContext);
			CurrentMesh = Command.RenderMesh;
		}

		D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = static_cast<D3D11_PRIMITIVE_TOPOLOGY>(Command.RenderMesh->Topology);
		if (DesiredTopology != CurrentTopology)
		{
			DeviceContext->IASetPrimitiveTopology(DesiredTopology);
			CurrentTopology = DesiredTopology;
		}

		Renderer.UpdateObjectConstantBuffer(Command.WorldMatrix);

		if (!Command.RenderMesh->Indices.empty())
		{
			UINT DrawCount = (Command.IndexCount > 0) ? Command.IndexCount : static_cast<UINT>(Command.RenderMesh->Indices.size());
			DeviceContext->DrawIndexed(DrawCount, Command.IndexStart, 0);
		}
		else
		{
			DeviceContext->Draw(static_cast<UINT>(Command.RenderMesh->Vertices.size()), 0);
		}
	}
}

void FSceneRenderer::AppendAdditionalCommands(FRenderCommandQueue& InOutQueue, const FRenderCommandQueue& AdditionalCommands)
{
	if (AdditionalCommands.Commands.empty())
	{
		return;
	}

	for (const FRenderCommand& Command : AdditionalCommands.Commands)
	{
		InOutQueue.AddCommand(Command);
	}
}

void FSceneRenderer::ApplyWireframeOverride(FRenderCommandQueue& InOutQueue, FMaterial* WireframeMaterial)
{
	if (!WireframeMaterial)
	{
		return;
	}

	for (FRenderCommand& Command : InOutQueue.Commands)
	{
		if (Command.RenderLayer != ERenderLayer::Overlay)
		{
			Command.Material = WireframeMaterial;
		}
	}
}

bool FSceneRenderer::RenderPacketToTarget(
	FRenderer& Renderer,
	ID3D11RenderTargetView* RenderTargetView,
	ID3D11DepthStencilView* DepthStencilView,
	const D3D11_VIEWPORT& Viewport,
	const FSceneRenderPacket& Packet,
	const FSceneViewRenderRequest& SceneView,
	const FRenderCommandQueue& AdditionalCommands,
	bool bForceWireframe,
	FMaterial* WireframeMaterial,
	const float ClearColor[4])
{
	FRenderCommandQueue Queue;
	BuildQueue(Renderer, Packet, SceneView, Queue);

	if (bForceWireframe)
	{
		ApplyWireframeOverride(Queue, WireframeMaterial);
	}

	AppendAdditionalCommands(Queue, AdditionalCommands);
	return RenderQueueToTarget(Renderer, RenderTargetView, DepthStencilView, Viewport, Queue, ClearColor);
}

bool FSceneRenderer::RenderQueueToTarget(
	FRenderer& Renderer,
	ID3D11RenderTargetView* RenderTargetView,
	ID3D11DepthStencilView* DepthStencilView,
	const D3D11_VIEWPORT& Viewport,
	const FRenderCommandQueue& Queue,
	const float ClearColor[4])
{
	ID3D11DeviceContext* Context = Renderer.GetDeviceContext();
	if (!Context || !RenderTargetView || !DepthStencilView)
	{
		return false;
	}

	Context->ClearRenderTargetView(RenderTargetView, ClearColor);
	Context->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	Context->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
	Context->RSSetViewports(1, &Viewport);
	SubmitCommands(Renderer, Queue);
	ExecuteCommands(Renderer);
	return true;
}
