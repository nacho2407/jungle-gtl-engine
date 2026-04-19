#include "Render/Builders//TextDrawCommandBuilder.h"

#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Scene/TextRenderSceneProxy.h"
#include "Component/TextRenderComponent.h"
#include "Resource/ResourceManager.h"

void FTextDrawCommandBuilder::BuildOverlay(FRenderPassContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Frame || !Context.OverlayTexts)
    {
        return;
    }

    const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
    if (!FontRes || !FontRes->IsLoaded())
    {
        return;
    }

    FFontBatch& FontBatch = Context.Renderer->GetFontBatch();
    FontBatch.ClearScreen();

    for (const FScene::FOverlayText& Text : *Context.OverlayTexts)
    {
        if (!Text.Text.empty())
        {
            FontBatch.AddScreenText(
                Text.Text,
                Text.Position.X,
                Text.Position.Y,
                Context.Frame->ViewportWidth,
                Context.Frame->ViewportHeight,
                Text.Scale);
        }
    }

    if (FontBatch.GetScreenIndexCount() == 0 || !FontBatch.UploadScreenBuffers(Context.Context))
    {
        return;
    }

    FShader* Shader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);
    if (!Shader)
    {
        return;
    }

    const FPassRenderState& State = Context.GetPassState(ERenderPass::OverlayFont);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = State.DepthStencil;
    Cmd.Blend = State.Blend;
    Cmd.Rasterizer = State.Rasterizer;
    Cmd.Topology = State.Topology;
    Cmd.RawVB = FontBatch.GetScreenVBBuffer();
    Cmd.RawVBStride = FontBatch.GetScreenVBStride();
    Cmd.RawIB = FontBatch.GetScreenIBBuffer();
    Cmd.IndexCount = FontBatch.GetScreenIndexCount();
    Cmd.DiffuseSRV = FontRes->SRV;
    Cmd.Pass = ERenderPass::OverlayFont;
    Cmd.DebugName = "OverlayText";
    Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}

void FTextDrawCommandBuilder::BuildWorld(const FTextRenderSceneProxy& Proxy, FRenderPassContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Frame || Proxy.CachedText.empty())
    {
        return;
    }

    const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(Proxy.Owner);

    const FFontResource* FontRes = TextComp ? TextComp->GetFont() : nullptr;
    if (!FontRes || !FontRes->IsLoaded())
    {
        FontRes = FResourceManager::Get().FindFont(FName("Default"));
    }
    if (!FontRes || !FontRes->IsLoaded())
    {
        return;
    }

    const FVector WorldScale = TextComp ? TextComp->GetWorldScale() : FVector(1.0f, 1.0f, 1.0f);

    FFontBatch& FontBatch = Context.Renderer->GetFontBatch();
    const uint32 StartIndex = FontBatch.GetWorldIndexCount();

    FontBatch.AddWorldText(
        Proxy.CachedText,
        TextComp ? TextComp->GetWorldLocation() : Proxy.CachedWorldPos,
        Context.Frame->CameraRight * -1.0f,
        Context.Frame->CameraUp,
        WorldScale,
        Proxy.CachedFontScale);

    const uint32 EndIndex = FontBatch.GetWorldIndexCount();
    if (EndIndex <= StartIndex || !FontBatch.UploadWorldBuffers(Context.Context))
    {
        return;
    }

    FShader* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
    if (!Shader)
    {
        return;
    }

    const FPassRenderState& State = Context.GetPassState(ERenderPass::AlphaBlend);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = State.DepthStencil;
    Cmd.Blend = State.Blend;
    Cmd.Rasterizer = State.Rasterizer;
    Cmd.Topology = State.Topology;
    Cmd.RawVB = FontBatch.GetWorldVBBuffer();
    Cmd.RawVBStride = FontBatch.GetWorldVBStride();
    Cmd.RawIB = FontBatch.GetWorldIBBuffer();
    Cmd.FirstIndex = StartIndex;
    Cmd.IndexCount = EndIndex - StartIndex;
    Cmd.DiffuseSRV = FontRes->SRV;
    Cmd.Pass = ERenderPass::AlphaBlend;
    Cmd.DebugName = "WorldText";
    Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}