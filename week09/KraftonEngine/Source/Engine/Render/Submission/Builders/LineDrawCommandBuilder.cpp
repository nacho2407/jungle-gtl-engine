#include "Render/Submission/Builders/LineDrawCommandBuilder.h"

#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Scene/Scene.h"
#include "Render/Passes/Common/PassRenderState.h"
#include "Render/Execution/Renderer.h"
#include "Render/Resources/Managers/ShaderManager.h"

void FLineDrawCommandBuilder::Build(FRenderPassContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.Frame)
    {
        return;
    }

    FLineBatch& EditorLines = Context.Renderer->GetEditorLineBatch();
    FLineBatch& GridLines = Context.Renderer->GetGridLineBatch();
    EditorLines.Clear();
    GridLines.Clear();

    if (Context.Scene->HasGrid())
    {
        GridLines.AddWorldHelpers(
            Context.Frame->ShowFlags,
            Context.Scene->GetGridSpacing(),
            Context.Scene->GetGridHalfLineCount(),
            Context.Frame->CameraPosition,
            Context.Frame->CameraForward,
            Context.Frame->bIsOrtho);
    }

    if (Context.DebugLines)
    {
        for (const FSceneDebugLine& Line : *Context.DebugLines)
        {
            EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
        }
    }

    if (const FShader* Shader = FShaderManager::Get().GetShader(EShaderType::Editor))
    {
        const FPassRenderState& State = Context.GetPassState(ERenderPass::EditorLines);

        auto AddBatch = [&](FLineBatch& Batch, const char* DebugName)
        {
            if (Batch.GetIndexCount() == 0 || !Batch.UploadBuffers(Context.Context))
            {
                return;
            }

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
            Cmd.Pass = ERenderPass::EditorLines;
            Cmd.DebugName = DebugName;
            Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, nullptr);
        };

        AddBatch(GridLines, "GridLines");
        AddBatch(EditorLines, "DebugLines");
    }
}
