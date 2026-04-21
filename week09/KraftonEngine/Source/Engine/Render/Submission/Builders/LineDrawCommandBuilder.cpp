#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Submission/Builders/LineDrawCommandBuilder.h"

#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Scene/Scene.h"
#include "Render/Passes/Base/PassRenderState.h"
#include "Render/Renderer.h"
#include "Render/Resources/ShaderManager.h"

namespace
{
void AddBatch(FRenderPipelineContext& Context, FDrawCommandList& OutList, FLineBatch& Batch, ERenderPass Pass, const char* DebugName)
{
    if (Batch.GetIndexCount() == 0 || !Batch.UploadBuffers(Context.Context))
    {
        return;
    }

    if (const FShader* Shader = FShaderManager::Get().GetShader(EShaderType::Editor))
    {
        const FPassRenderState& State = Context.GetPassState(Pass);

        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader = const_cast<FShader*>(Shader);
        Cmd.DepthStencil = State.DepthStencil;
        Cmd.Blend = State.Blend;
        Cmd.Rasterizer = State.Rasterizer;
        Cmd.Topology = State.Topology;
        Cmd.RawVB = Batch.GetVBBuffer();
        Cmd.RawVBStride = Batch.GetVBStride();
        Cmd.RawIB = Batch.GetIBBuffer();
        Cmd.IndexCount = Batch.GetIndexCount();
        Cmd.Pass = Pass;
        Cmd.DebugName = DebugName;
        Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, nullptr);
    }
}
} // namespace

void FLineDrawCommandBuilder::BuildGrid(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.SceneView)
    {
        return;
    }

    FLineBatch& GridLines = Context.Renderer->GetGridLineBatch();
    GridLines.Clear();

    if (Context.Scene->HasGrid())
    {
        GridLines.AddWorldHelpers(
            Context.SceneView->ShowFlags,
            Context.Scene->GetGridSpacing(),
            Context.Scene->GetGridHalfLineCount(),
            Context.SceneView->CameraPosition,
            Context.SceneView->CameraForward,
            Context.SceneView->bIsOrtho);
    }

    AddBatch(Context, OutList, GridLines, ERenderPass::Grid, "GridLines");
}

void FLineDrawCommandBuilder::BuildDebugLines(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.SceneView)
    {
        return;
    }

    FLineBatch& EditorLines = Context.Renderer->GetEditorLineBatch();
    EditorLines.Clear();

    if (Context.DebugLines)
    {
        for (const FSceneDebugLine& Line : *Context.DebugLines)
        {
            EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
        }
    }

    AddBatch(Context, OutList, EditorLines, ERenderPass::EditorLines, "DebugLines");
}
