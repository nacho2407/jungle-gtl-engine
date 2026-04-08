#include "Renderer/SceneRenderer.h"

#include <algorithm>

#include "Renderer/Material.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"

void FSceneRenderer::BeginFrame()
{
	PrevCommandCount = (std::max)(PrevCommandCount, CurrentFramePeakCommandCount);
	CurrentFramePeakCommandCount = 0;
	DefaultCommandList.clear();
	OverlayCommandList.clear();
	TransparentCommandList.clear();
	DefaultCommandList.reserve(PrevCommandCount);
	OverlayCommandList.reserve((PrevCommandCount / 4) + 1);
	TransparentCommandList.reserve((PrevCommandCount / 4) + 1);
	NextSubmissionOrder = 0;
}

size_t FSceneRenderer::GetPrevCommandCount() const
{
	return (std::max)(PrevCommandCount, CurrentFramePeakCommandCount);
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
	BuildContext.BillboardFeature = Renderer.GetSceneBillboardFeature();
	BuildContext.TotalTimeSeconds = SceneView.TotalTimeSeconds;

	SceneCommandBuilder.BuildQueue(BuildContext, Packet, SceneView.CameraPosition, OutQueue);
}

void FSceneRenderer::AddCommand(FRenderer& Renderer, TArray<FRenderCommand>& TargetList, const FRenderCommand& Command)
{
	TargetList.push_back(Command);
	FRenderCommand& Added = TargetList.back();
	if (!Added.Material)
	{
		Added.Material = Renderer.GetDefaultMaterial();
	}

	Added.SortKey = FRenderCommand::MakeSortKey(Added.Material, Added.RenderMesh);
	Added.SubmissionOrder = NextSubmissionOrder++;
}

void FSceneRenderer::ClearCommandLists()
{
	const size_t PrevDefaultCount = DefaultCommandList.size();
	const size_t PrevOverlayCount = OverlayCommandList.size();
	const size_t PrevTransparentCount = TransparentCommandList.size();
	const size_t ExecutedCommandCount = PrevDefaultCount + PrevOverlayCount + PrevTransparentCount;
	CurrentFramePeakCommandCount = (std::max)(CurrentFramePeakCommandCount, ExecutedCommandCount);

	DefaultCommandList.clear();
	OverlayCommandList.clear();
	TransparentCommandList.clear();

	DefaultCommandList.reserve(PrevDefaultCount);
	OverlayCommandList.reserve(PrevOverlayCount);
	TransparentCommandList.reserve(PrevTransparentCount);
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

	auto SubmitBucket = [&](const TArray<FRenderCommand>& SourceCommands, TArray<FRenderCommand>& TargetList)
	{
		for (const FRenderCommand& Command : SourceCommands)
		{
			if (Command.RenderMesh)
			{
				Command.RenderMesh->UpdateVertexAndIndexBuffer(Device, DeviceContext);
			}

			AddCommand(Renderer, TargetList, Command);
		}
	};

	SubmitBucket(Queue.DefaultCommands, DefaultCommandList);
	SubmitBucket(Queue.OverlayCommands, OverlayCommandList);
	SubmitBucket(Queue.TransparentCommands, TransparentCommandList);
}

void FSceneRenderer::SortRenderPass(TArray<FRenderCommand>& Commands, ERenderLayer RenderLayer)
{
	if (Commands.size() < 2)
	{
		return;
	}

	switch (RenderLayer)
	{
	case ERenderLayer::Default:
		std::sort(
			Commands.begin(),
			Commands.end(),
			[](const FRenderCommand& A, const FRenderCommand& B)
			{
				if (A.SortKey != B.SortKey)
				{
					return A.SortKey < B.SortKey;
				}
				return A.SubmissionOrder < B.SubmissionOrder;
			});
		break;

	case ERenderLayer::Transparent:
		// 투명체는 이후 깊이 기반 정렬이 들어갈 자리다.
		std::sort(
			Commands.begin(),
			Commands.end(),
			[](const FRenderCommand& A, const FRenderCommand& B)
			{
				return A.SubmissionOrder < B.SubmissionOrder;
			});
		break;

	case ERenderLayer::Overlay:
	default:
		// Overlay는 제출 순서를 유지해 기즈모/디버그 출력 순서를 보존한다.
		break;
	}
}

void FSceneRenderer::ExecuteCommands(FRenderer& Renderer)
{
	Renderer.SetConstantBuffers();
	Renderer.UpdateFrameConstantBuffer();

	SortRenderPass(DefaultCommandList, ERenderLayer::Default);
	ExecuteRenderPass(Renderer, DefaultCommandList);

	SortRenderPass(TransparentCommandList, ERenderLayer::Transparent);
	ExecuteRenderPass(Renderer, TransparentCommandList);

	Renderer.ClearDepthBuffer();

	SortRenderPass(OverlayCommandList, ERenderLayer::Overlay);
	ExecuteRenderPass(Renderer, OverlayCommandList);

	ClearCommandLists();
}

void FSceneRenderer::ExecuteRenderPass(FRenderer& Renderer, const TArray<FRenderCommand>& Commands)
{
	if (Commands.empty())
	{
		return;
	}

	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	FMaterial* CurrentMaterial = nullptr;
	FRenderMesh* CurrentMesh = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	Renderer.GetRenderStateManager()->RebindState();
	for (const FRenderCommand& Command : Commands)
	{
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

		const D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = static_cast<D3D11_PRIMITIVE_TOPOLOGY>(Command.RenderMesh->Topology);
		if (DesiredTopology != CurrentTopology)
		{
			DeviceContext->IASetPrimitiveTopology(DesiredTopology);
			CurrentTopology = DesiredTopology;
		}

		Renderer.UpdateObjectConstantBuffer(Command.WorldMatrix);

		if (!Command.RenderMesh->Indices.empty())
		{
			const UINT DrawCount = (Command.IndexCount > 0) ? Command.IndexCount : static_cast<UINT>(Command.RenderMesh->Indices.size());
			DeviceContext->DrawIndexed(DrawCount, Command.IndexStart, 0);
		}
		else
		{
			DeviceContext->Draw(static_cast<UINT>(Command.RenderMesh->Vertices.size()), 0);
		}
	}
}

void FSceneRenderer::ApplyWireframeOverride(FRenderCommandQueue& InOutQueue, FMaterial* WireframeMaterial)
{
	if (!WireframeMaterial)
	{
		return;
	}

	for (FRenderCommand& Command : InOutQueue.DefaultCommands)
	{
		Command.Material = WireframeMaterial;
	}

	for (FRenderCommand& Command : InOutQueue.TransparentCommands)
	{
		Command.Material = WireframeMaterial;
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
	FRenderCommandQueue SceneQueue;
	BuildQueue(Renderer, Packet, SceneView, SceneQueue);

	if (bForceWireframe)
	{
		ApplyWireframeOverride(SceneQueue, WireframeMaterial);
	}

	// 추가 커맨드를 같은 프레임 큐에 합쳐 깊이 버퍼 연속성을 유지한다.
	SceneQueue.Append(AdditionalCommands);

	return RenderQueueToTarget(Renderer, RenderTargetView, DepthStencilView, Viewport, SceneQueue, ClearColor, true);
}

bool FSceneRenderer::RenderQueueToTarget(
	FRenderer& Renderer,
	ID3D11RenderTargetView* RenderTargetView,
	ID3D11DepthStencilView* DepthStencilView,
	const D3D11_VIEWPORT& Viewport,
	const FRenderCommandQueue& Queue,
	const float ClearColor[4],
	bool bClearTarget)
{
	ID3D11DeviceContext* Context = Renderer.GetDeviceContext();
	if (!Context || !RenderTargetView || !DepthStencilView)
	{
		return false;
	}

	if (bClearTarget)
	{
		Context->ClearRenderTargetView(RenderTargetView, ClearColor);
		Context->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	Context->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
	Context->RSSetViewports(1, &Viewport);

	if (Queue.IsEmpty())
	{
		return true;
	}

	SubmitCommands(Renderer, Queue);
	ExecuteCommands(Renderer);
	return true;
}
