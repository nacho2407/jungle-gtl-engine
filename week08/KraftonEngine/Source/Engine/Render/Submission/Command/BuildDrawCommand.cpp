#include "Render/Submission/Command/BuildDrawCommand.h"

#include "Component/TextRenderComponent.h"
#include "Render/Passes/Scene/ShadingTypes.h"
#include "Render/Passes/Base/PassRenderState.h"
#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/Pipelines/Context/FrameRenderResources.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Pipelines/Context/ViewMode/SceneViewModeSurfaces.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Renderer.h"
#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Resource/ResourceManager.h"


namespace
{
bool TryResolveViewModeStage(ERenderPass Pass, EPipelineStage& OutStage)
{
    switch (Pass)
    {
    case ERenderPass::Opaque:
        OutStage = EPipelineStage::BaseDraw;
        return true;
    case ERenderPass::Decal:
        OutStage = EPipelineStage::Decal;
        return true;
    case ERenderPass::Lighting:
        OutStage = EPipelineStage::Lighting;
        return true;
    default:
        return false;
    }
}
} // namespace

void DrawCommandBuilder::BuildMeshDrawCommand(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    const bool bHasMeshBuffer = (Proxy.MeshBuffer != nullptr);
    const bool bMeshValid = bHasMeshBuffer && Proxy.MeshBuffer->IsValid();

    if (!bMeshValid)
    {
        return;
    }

    ID3D11DeviceContext* Ctx = Context.Context;
    FShader* Shader = Proxy.Shader;
    const bool bIsMaskLikePass = (Pass == ERenderPass::DepthPre || Pass == ERenderPass::SelectionMask);

    if (bIsMaskLikePass)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::DepthOnly);
    }
    else if (Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        EPipelineStage ViewModeStage;
        const bool bSupportsViewModeStage = TryResolveViewModeStage(Pass, ViewModeStage);
        const bool bRequiresViewModeSurface = (Pass == ERenderPass::Opaque || Pass == ERenderPass::Decal);

        if (Proxy.bAllowViewModeShaderOverride && bSupportsViewModeStage && (!bRequiresViewModeSurface || Context.ActiveViewSurfaces))
        {
            if (const FRenderPipelinePassDesc* Desc = Context.ViewModePassRegistry->FindPassDesc(Context.ActiveViewMode, ViewModeStage))
            {
                if (Desc->CompiledShader)
                {
                    Shader = Desc->CompiledShader;
                }
            }
        }
    }

    if (!Shader)
    {
        return;
    }

    const FPassRenderStateDesc& PassState = Context.GetPassState(Pass);

    FConstantBuffer* PerObjCB = Context.Renderer ? Context.Renderer->AcquirePerObjectCBForProxy(Proxy) : nullptr;
    if (!PerObjCB && Context.Resources)
    {
        PerObjCB = &Context.Resources->PerObjectConstantBuffer;
    }

    if (PerObjCB && Ctx && Proxy.NeedsPerObjectCBUpload())
    {
        PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
        Proxy.ClearPerObjectCBDirty();
    }

    FConstantBuffer* ExtraCB0 = nullptr;
    FConstantBuffer* ExtraCB1 = nullptr;
    if (!bIsMaskLikePass && Proxy.ExtraCB.Buffer && Proxy.ExtraCB.Size > 0 && Ctx)
    {
        Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);

        if (Proxy.ExtraCB.Slot == ECBSlot::PerShader0)
        {
            ExtraCB0 = Proxy.ExtraCB.Buffer;
        }
        else if (Proxy.ExtraCB.Slot == ECBSlot::PerShader1)
        {
            ExtraCB1 = Proxy.ExtraCB.Buffer;
        }
    }

    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* BaseSRV, ID3D11ShaderResourceView* InNormalSRV,
                          ID3D11ShaderResourceView* InSpecularSRV,
                          FConstantBuffer* CB0, FConstantBuffer* CB1,
                          EBlendState SectionBlend, EDepthStencilState SectionDepthStencil, ERasterizerState SectionRasterizer)
    {
        if (IndexCount == 0)
        {
            return;
        }

        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader = Shader;
        Cmd.MeshBuffer = Proxy.MeshBuffer;
        Cmd.FirstIndex = FirstIndex;
        Cmd.IndexCount = IndexCount;

        if (bIsMaskLikePass)
        {
            Cmd.DepthStencil = PassState.DepthStencil;
            Cmd.Blend = PassState.Blend;
            Cmd.Rasterizer = PassState.Rasterizer;
        }
        else
        {

            const bool bUsesViewModeBaseDraw =
                (Pass == ERenderPass::Opaque) &&
                Context.ViewModePassRegistry &&
                Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode);

            // Reversed-Z depth pre-pass writes the same geometry depth first.
            // BaseDraw must then use GREATER_EQUAL-style read-only depth or every
            // opaque pixel at the exact same depth fails the comparison.
            if (SectionDepthStencil != EDepthStencilState::Default)
            {
                Cmd.DepthStencil = SectionDepthStencil;
            }
            else if (bUsesViewModeBaseDraw)
            {
                Cmd.DepthStencil = EDepthStencilState::DepthReadOnly;
            }
            else
            {
                Cmd.DepthStencil = PassState.DepthStencil;
            }

            if (SectionBlend != EBlendState::Opaque || Pass != ERenderPass::Opaque)
            {
                Cmd.Blend = SectionBlend;
            }
            else
            {
                Cmd.Blend = PassState.Blend;
            }

            if (SectionRasterizer != ERasterizerState::SolidBackCull)
            {
                Cmd.Rasterizer = SectionRasterizer;
            }
            else
            {
                Cmd.Rasterizer = PassState.Rasterizer;
            }
        }

        if (Pass == ERenderPass::Opaque &&
            Context.ActiveViewMode == EViewMode::Wireframe)
        {
            Cmd.Rasterizer = ERasterizerState::WireFrame;
        }

        Cmd.Topology = PassState.Topology;
        Cmd.PerObjectCB = PerObjCB;
        Cmd.PerShaderCB[0] = bIsMaskLikePass ? nullptr : (CB0 ? CB0 : (Proxy.MaterialCB[0] ? Proxy.MaterialCB[0] : ExtraCB0));
        Cmd.PerShaderCB[1] = bIsMaskLikePass ? nullptr : (CB1 ? CB1 : (Proxy.MaterialCB[1] ? Proxy.MaterialCB[1] : ExtraCB1));
        Cmd.LightCB = (bIsMaskLikePass || !Context.Resources) ? nullptr : &Context.Resources->GlobalLightBuffer;
        Cmd.LocalLightSRV = (bIsMaskLikePass || !Context.Resources) ? nullptr : Context.Resources->LocalLightSRV;
        Cmd.DiffuseSRV = bIsMaskLikePass ? nullptr : BaseSRV;
        Cmd.NormalSRV = bIsMaskLikePass ? nullptr : InNormalSRV;
        Cmd.SpecularSRV = bIsMaskLikePass ? nullptr : InSpecularSRV;
        Cmd.Pass = Pass;
        const uintptr_t MaterialHash =
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[0]) >> 4) ^
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[1]) >> 9) ^
            (reinterpret_cast<uintptr_t>(Cmd.DiffuseSRV) >> 14) ^
            (reinterpret_cast<uintptr_t>(Cmd.NormalSRV) >> 19) ^
            (reinterpret_cast<uintptr_t>(Cmd.SpecularSRV) >> 24);
        Cmd.SortKey = FDrawCommand::BuildSortKey(
            Pass,
            Cmd.Shader,
            Proxy.MeshBuffer,
            Cmd.DiffuseSRV,
            static_cast<uint16>(MaterialHash & 0x0FFFu));
    };

    if (!Proxy.SectionRenderData.empty())
    {
        for (const FMeshSectionRenderData& S : Proxy.SectionRenderData)
        {
            AddSection(
                S.FirstIndex,
                S.IndexCount,
                S.DiffuseSRV,
                S.NormalSRV,
                S.SpecularSRV,
                S.MaterialCB[0],
                S.MaterialCB[1],
                S.Blend,
                S.DepthStencil,
                S.Rasterizer);
        }
    }
    else
    {
        AddSection(
            0,
            Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount(),
            Proxy.DiffuseSRV,
            Proxy.NormalSRV,
            Proxy.SpecularSRV,
            Proxy.MaterialCB[0],
            Proxy.MaterialCB[1],
            Proxy.Blend,
            Proxy.DepthStencil,
            Proxy.Rasterizer);
    }
}


void DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    FShader* Shader = nullptr;

    if (Pass == ERenderPass::Lighting)
    {
        if (!Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
        {
            return;
        }

        const FRenderPipelinePassDesc* Desc = Context.ViewModePassRegistry->FindPassDesc(Context.ActiveViewMode, EPipelineStage::Lighting);
        if (!Desc || !Desc->CompiledShader)
        {
            return;
        }

        Shader = Desc->CompiledShader;
    }
    else if (Pass == ERenderPass::FXAA)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::FXAA);
    }
    else if (Pass == ERenderPass::PostProcess)
    {
        switch (PostProcessVariant)
        {
        case EViewModePostProcessVariant::Outline:
            Shader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
            break;
        case EViewModePostProcessVariant::SceneDepth:
            Shader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
            break;
        case EViewModePostProcessVariant::WorldNormal:
            Shader = FShaderManager::Get().GetShader(EShaderType::NormalView);
            break;
        case EViewModePostProcessVariant::LightHitMap:
            Shader = FShaderManager::Get().GetShader(EShaderType::LightHitMap);
            break;
        default:
            Shader = FShaderManager::Get().GetShader(EShaderType::HeightFog);
            break;
        }
    }

    if (!Shader)
        return;

    const FPassRenderStateDesc& S = Context.GetPassState(Pass);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = S.DepthStencil;
    Cmd.Blend = S.Blend;
    Cmd.Rasterizer = S.Rasterizer;
    Cmd.Topology = S.Topology;
    Cmd.VertexCount = 3;
    Cmd.Pass = Pass;

    if (Pass == ERenderPass::Lighting && Context.ActiveViewSurfaces)
    {
        // Lighting fullscreen shaders read the base color buffer from t0.
        Cmd.DiffuseSRV = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::BaseColor);
    }
    else if (Pass == ERenderPass::FXAA && Context.SceneView)
    {
        // FXAA prepares SceneColor on t0 before submission; keep the command in sync
        // so SubmitCommand does not overwrite it with nullptr on a forced bind.
        Cmd.DiffuseSRV = Targets ? Targets->SceneColorCopySRV : nullptr;
    }

    Cmd.LightCB = (Pass == ERenderPass::Lighting && Context.Resources) ? &Context.Resources->GlobalLightBuffer : nullptr;
    Cmd.LocalLightSRV = (Pass == ERenderPass::Lighting && Context.Resources) ? Context.Resources->LocalLightSRV : nullptr;
    Cmd.SortKey = FDrawCommand::BuildSortKey(Pass, Shader, nullptr, Cmd.DiffuseSRV, ToPostProcessUserBits(PostProcessVariant));
}


void DrawCommandBuilder::BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.SceneView)
    {
        return;
    }

    FLineBatch& EditorLines = Context.Renderer->GetEditorLineBatch();
    FLineBatch& GridLines = Context.Renderer->GetGridLineBatch();
    EditorLines.Clear();
    GridLines.Clear();

    if (Context.OverlayData && Context.OverlayData->HasGrid())
    {
        GridLines.AddWorldHelpers(
            Context.SceneView->ShowFlags,
            Context.OverlayData->GetGridSpacing(),
            Context.OverlayData->GetGridHalfLineCount(),
            Context.SceneView->CameraPosition,
            Context.SceneView->CameraForward,
            Context.SceneView->bIsOrtho);
    }

    if (Context.DebugLines)
    {
        for (const FSceneDebugLine& Line : *Context.DebugLines)
        {
            EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
        }
    }

    if (Context.OverlayData)
    {
        for (const FSceneDebugAABB& Box : Context.OverlayData->GetDebugAABBs())
        {
            const FVector& Min = Box.Min;
            const FVector& Max = Box.Max;
            const FVector V[8] = {
                FVector(Min.X, Min.Y, Min.Z),
                FVector(Max.X, Min.Y, Min.Z),
                FVector(Max.X, Max.Y, Min.Z),
                FVector(Min.X, Max.Y, Min.Z),
                FVector(Min.X, Min.Y, Max.Z),
                FVector(Max.X, Min.Y, Max.Z),
                FVector(Max.X, Max.Y, Max.Z),
                FVector(Min.X, Max.Y, Max.Z),
            };
            static constexpr int32 Edges[][2] = {
                { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
            };
            for (const auto& Edge : Edges)
            {
                EditorLines.AddLine(V[Edge[0]], V[Edge[1]], Box.Color.ToVector4());
            }
        }
    }

    if (const FShader* Shader = FShaderManager::Get().GetShader(EShaderType::Editor))
    {
        const FPassRenderStateDesc& State = Context.GetPassState(ERenderPass::EditorLines);

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
            Cmd.Rasterizer = ERasterizerState::SolidNoCull;
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


void DrawCommandBuilder::BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.OverlayBillboardProxies)
    {
        return;
    }

    for (FPrimitiveSceneProxy* Proxy : *Context.OverlayBillboardProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        BuildMeshDrawCommand(*Proxy, ERenderPass::OverlayBillboard, Context, OutList);
    }
}

void DrawCommandBuilder::BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView)
    {
        return;
    }

    if (Context.OverlayTextProxies)
    {
        for (FPrimitiveSceneProxy* Proxy : *Context.OverlayTextProxies)
        {
            if (!Proxy)
            {
                continue;
            }

            const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
            if (!TextProxy->CachedText.empty())
            {
                BuildOverlayWorldTextDrawCommand(*TextProxy, Context, OutList);
            }
        }
    }

    if (!Context.OverlayTexts)
    {
        return;
    }

    const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
    if (!FontRes || !FontRes->IsLoaded())
    {
        return;
    }

    FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
    FontBatch.EnsureCharInfoMap(FontRes);
    FontBatch.ClearScreen();

    for (const FSceneOverlayText& Text : *Context.OverlayTexts)
    {
        if (!Text.Text.empty())
        {
            FontBatch.AddScreenText(
                Text.Text,
                Text.Position.X,
                Text.Position.Y,
                Context.SceneView->ViewportWidth,
                Context.SceneView->ViewportHeight,
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

    const FPassRenderStateDesc& State = Context.GetPassState(ERenderPass::OverlayFont);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = State.DepthStencil;
    Cmd.Blend = State.Blend;
    Cmd.Rasterizer = ERasterizerState::SolidNoCull;
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

void DrawCommandBuilder::BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView || Proxy.CachedText.empty())
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

    FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
    FontBatch.EnsureCharInfoMap(FontRes);
    const uint32 StartIndex = FontBatch.GetWorldIndexCount();

    FontBatch.AddWorldText(
        Proxy.CachedText,
        TextComp ? TextComp->GetWorldLocation() : Proxy.CachedWorldPos,
        Proxy.CachedTextRight,
        Proxy.CachedTextUp,
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

    const FPassRenderStateDesc& State = Context.GetPassState(ERenderPass::AlphaBlend);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = State.DepthStencil;
    Cmd.Blend = State.Blend;
    Cmd.Rasterizer = ERasterizerState::SolidNoCull;
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

void DrawCommandBuilder::BuildOverlayWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView || Proxy.CachedText.empty())
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

    FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
    FontBatch.EnsureCharInfoMap(FontRes);
    const uint32 StartIndex = FontBatch.GetOverlayWorldIndexCount();

    FontBatch.AddOverlayWorldText(
        Proxy.CachedText,
        TextComp ? TextComp->GetWorldLocation() : Proxy.CachedWorldPos,
        Proxy.CachedTextRight,
        Proxy.CachedTextUp,
        WorldScale,
        Proxy.CachedFontScale);

    const uint32 EndIndex = FontBatch.GetOverlayWorldIndexCount();
    if (EndIndex <= StartIndex || !FontBatch.UploadOverlayWorldBuffers(Context.Context))
    {
        return;
    }

    FShader* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
    if (!Shader)
    {
        return;
    }

    const FPassRenderStateDesc& State = Context.GetPassState(ERenderPass::OverlayTextWorld);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = State.DepthStencil;
    Cmd.Blend = State.Blend;
    Cmd.Rasterizer = ERasterizerState::SolidNoCull;
    Cmd.Topology = State.Topology;
    Cmd.RawVB = FontBatch.GetOverlayWorldVBBuffer();
    Cmd.RawVBStride = FontBatch.GetOverlayWorldVBStride();
    Cmd.RawIB = FontBatch.GetOverlayWorldIBBuffer();
    Cmd.FirstIndex = StartIndex;
    Cmd.IndexCount = EndIndex - StartIndex;
    Cmd.DiffuseSRV = FontRes->SRV;
    Cmd.Pass = ERenderPass::OverlayTextWorld;
    Cmd.DebugName = "OverlayWorldText";
    Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}


void DrawCommandBuilder::BuildDecalDrawCommand(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Proxy.DiffuseSRV)
        return;
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Proxy.Shader;
    Cmd.DepthStencil = EDepthStencilState::NoDepth;
    Cmd.Blend = EBlendState::AlphaBlend;
    Cmd.Rasterizer = ERasterizerState::SolidNoCull;
    Cmd.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Cmd.VertexCount = 3;
    Cmd.DiffuseSRV = Proxy.DiffuseSRV;
    Cmd.Pass = ERenderPass::Decal;
    Cmd.SortKey = FDrawCommand::BuildSortKey(ERenderPass::Decal, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}

void DrawCommandBuilder::BuildDecalReceiverDrawCommand(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!ReceiverProxy.MeshBuffer)
        return;
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = DecalProxy.Shader;
    Cmd.MeshBuffer = ReceiverProxy.MeshBuffer;
    Cmd.IndexCount = ReceiverProxy.MeshBuffer->GetIndexBuffer().GetIndexCount();
    Cmd.Blend = DecalProxy.Blend;
    Cmd.DepthStencil = DecalProxy.DepthStencil;
    Cmd.Rasterizer = DecalProxy.Rasterizer;
    Cmd.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Cmd.DiffuseSRV = DecalProxy.DiffuseSRV;
    Cmd.Pass = DecalProxy.Pass;
    Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, Cmd.MeshBuffer, Cmd.DiffuseSRV);
}
