#include "Render/Pass/UberLitPass.h"

#include "Render/Pipeline/PipelineShaderResolver.h"
#include "Render/Pipeline/ViewModeRenderPipeline.h"

FDrawCommand* BuildUberLitPassCommand(
	FDrawCommandList& DrawCommandList,
	const FPassRenderState (&PassRenderStates)[(uint32)ERenderPass::MAX],
	const FViewModeRenderPipeline* ActiveViewPipeline)
{
	if (!ActiveViewPipeline)
	{
		return nullptr;
	}

	FShader* LightingShader = ResolvePipelineShader(ActiveViewPipeline, ERenderPass::Lighting, nullptr);
	if (!LightingShader)
	{
		return nullptr;
	}

	const FPassRenderState& LightingState = PassRenderStates[(uint32)ERenderPass::Lighting];

	FDrawCommand& Cmd = DrawCommandList.AddCommand();
	Cmd.Shader = LightingShader;
	Cmd.DepthStencil = LightingState.DepthStencil;
	Cmd.Blend = LightingState.Blend;
	Cmd.Rasterizer = LightingState.Rasterizer;
	Cmd.Topology = LightingState.Topology;
	Cmd.VertexCount = 3;
	Cmd.Pass = ERenderPass::Lighting;
	Cmd.SortKey = FDrawCommand::BuildSortKey(ERenderPass::Lighting, Cmd.Shader, nullptr, nullptr, 0);
	return &Cmd;
}
