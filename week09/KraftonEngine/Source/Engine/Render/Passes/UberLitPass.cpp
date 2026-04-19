#include "Render/Passes/UberLitPass.h"
#include "Render/Commands/DrawCommandList.h"

#include "Render/Renderer/PipelineShaderResolver.h"
#include "Render/Core/PassTypes.h"

FDrawCommand* BuildUberLitPassCommand(
	FDrawCommandList& DrawCommandList,
	const FPassRenderState (&PassRenderStates)[(uint32)ERenderPass::MAX],
	const FViewModePassRegistry* ViewModePassRegistry,
	EViewMode ActiveViewMode)
{
	if (!ViewModePassRegistry || !ViewModePassRegistry->HasConfig(ActiveViewMode))
	{
		return nullptr;
	}

	FShader* LightingShader = ResolvePipelineShader(ViewModePassRegistry, ActiveViewMode, ERenderPass::Lighting, nullptr);
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
