// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Command/BuildDrawCommand.h"

#include "Component/TextRenderComponent.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/ViewMode/ShadingModel.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/RenderPassPresets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Renderer.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Resource/ResourceManager.h"

void DrawCommandBuild::BuildMeshDrawCommand(const FPrimitiveProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, uint16 UserBits)
{
    const bool bHasMeshBuffer = (Proxy.MeshBuffer != nullptr);
    const bool bMeshValid     = bHasMeshBuffer && Proxy.MeshBuffer->IsValid();

    if (!bMeshValid)
    {
        return;
    }

    ID3D11DeviceContext* Ctx             = Context.Context;
    FGraphicsProgram*    Shader          = Proxy.Shader;
    const bool           bIsMaskLikePass = (Pass == ERenderPass::DepthPre || Pass == ERenderPass::SelectionMask || Pass == ERenderPass::ShadowMap);

    if (bIsMaskLikePass)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::DepthOnly);
    }
    else if (Pass == ERenderPass::Opaque &&
             Context.SceneView &&
             Proxy.bAllowViewModeShaderOverride &&
             Context.ViewMode.Registry &&
             Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        const FViewModePassDesc* Desc =
            Context.ViewMode.Registry->FindPassDesc(Context.ViewMode.ActiveViewMode, ERenderPass::Opaque, Context.SceneView->RenderPath);
        if (Desc && Desc->CompiledShader)
        {
            Shader = Desc->CompiledShader;
        }
    }

    if (!Shader)
    {
        return;
    }

    const FRenderPassDrawPreset& PassPreset = Context.GetRenderPassDrawPreset(Pass);

    FConstantBuffer* PerObjCB = Context.Renderer ? Context.Renderer->AcquirePerObjectCBForProxy(Proxy) : nullptr;
    if (!PerObjCB && Context.Resources)
    {
        PerObjCB = &Context.Resources->PerObjectConstantBuffer;
    }

    if (PerObjCB && Ctx && Proxy.NeedsPerObjectCBUpload())
    {
        PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectCBData));
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
        Cmd.Shader        = Shader;
        Cmd.MeshBuffer    = Proxy.MeshBuffer;
        Cmd.FirstIndex    = FirstIndex;
        Cmd.IndexCount    = IndexCount;

        if (bIsMaskLikePass)
        {
            Cmd.DepthStencil = PassPreset.DepthStencil;
            Cmd.Blend        = PassPreset.Blend;
            Cmd.Rasterizer   = PassPreset.Rasterizer;
        }
        else
        {
            const bool bUsesViewModeOpaque =
                (Pass == ERenderPass::Opaque) &&
                Context.ViewMode.Registry &&
                Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode);

            // Reversed-Z depth pre-pass writes the same geometry depth first.
            // Opaque must then use GREATER_EQUAL-style read-only depth or every
            // opaque pixel at the exact same depth fails the comparison.
            if (SectionDepthStencil != EDepthStencilState::Default)
            {
                Cmd.DepthStencil = SectionDepthStencil;
            }
            else if (bUsesViewModeOpaque)
            {
                Cmd.DepthStencil = EDepthStencilState::DepthReadOnly;
            }
            else
            {
                Cmd.DepthStencil = PassPreset.DepthStencil;
            }

            if (SectionBlend != EBlendState::Opaque || Pass != ERenderPass::Opaque)
            {
                Cmd.Blend = SectionBlend;
            }
            else
            {
                Cmd.Blend = PassPreset.Blend;
            }

            if (SectionRasterizer != ERasterizerState::SolidBackCull)
            {
                Cmd.Rasterizer = SectionRasterizer;
            }
            else
            {
                Cmd.Rasterizer = PassPreset.Rasterizer;
            }
        }

        if ((Pass == ERenderPass::Opaque ||
             Pass == ERenderPass::AlphaBlend ||
             Pass == ERenderPass::OverlayBillboard ||
             Pass == ERenderPass::OverlayTextWorld) &&
            Context.ViewMode.ActiveViewMode == EViewMode::Wireframe)
        {
            Cmd.Rasterizer = ERasterizerState::WireFrame;
        }

        Cmd.Topology       = PassPreset.Topology;
        Cmd.PerObjectCB    = PerObjCB;
        Cmd.PerShaderCB[0] = bIsMaskLikePass ? nullptr : (CB0 ? CB0 : (Proxy.MaterialCB[0] ? Proxy.MaterialCB[0] : ExtraCB0));
        Cmd.PerShaderCB[1] = bIsMaskLikePass ? nullptr : (CB1 ? CB1 : (Proxy.MaterialCB[1] ? Proxy.MaterialCB[1] : ExtraCB1));
        Cmd.LightCB        = (bIsMaskLikePass || !Context.Resources) ? nullptr : &Context.Resources->GlobalLightBuffer;
        Cmd.LocalLightSRV  = (bIsMaskLikePass || !Context.Resources) ? nullptr : Context.Resources->LocalLightSRV;
        Cmd.DiffuseSRV     = bIsMaskLikePass ? nullptr : BaseSRV;
        Cmd.NormalSRV      = bIsMaskLikePass ? nullptr : InNormalSRV;
        Cmd.SpecularSRV    = bIsMaskLikePass ? nullptr : InSpecularSRV;
        Cmd.Pass           = Pass;
        const uintptr_t MaterialHash =
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[0]) >> 4) ^
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[1]) >> 9) ^
            (reinterpret_cast<uintptr_t>(Cmd.DiffuseSRV) >> 14) ^
            (reinterpret_cast<uintptr_t>(Cmd.NormalSRV) >> 19) ^
            (reinterpret_cast<uintptr_t>(Cmd.SpecularSRV) >> 24);

        // 수동 정렬이 필요한 경우 (e.g. Light 별 섀도 맵 렌더링) UserBits 사용하여 정렬.
        // 아니라면 Material 별로 정렬
        const uint16 AdditionalSortKey = (UserBits != 0) ? UserBits : static_cast<uint16>(MaterialHash & 0x0FFFu);

        Cmd.SortKey = FDrawCommand::BuildSortKey(
            Pass,
            Cmd.Shader,
            Proxy.MeshBuffer,
            Cmd.DiffuseSRV,
            AdditionalSortKey);
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


void DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    FGraphicsProgram*             Shader  = nullptr;

    if (Pass == ERenderPass::DeferredLighting)
    {
        if (!Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
        {
            return;
        }

        const FViewModePassDesc* Desc =
            Context.ViewMode.Registry->FindPassDesc(Context.ViewMode.ActiveViewMode, ERenderPass::DeferredLighting, ERenderShadingPath::Deferred);
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

    const FRenderPassDrawPreset& S   = Context.GetRenderPassDrawPreset(Pass);
    FDrawCommand&                Cmd = OutList.AddCommand();
    Cmd.Shader                       = Shader;
    Cmd.DepthStencil                 = S.DepthStencil;
    Cmd.Blend                        = S.Blend;
    Cmd.Rasterizer                   = S.Rasterizer;
    Cmd.Topology                     = S.Topology;
    Cmd.VertexCount                  = 3;
    Cmd.Pass                         = Pass;

    if (Pass == ERenderPass::DeferredLighting && Context.ViewMode.Surfaces)
    {
        // Lighting fullscreen shaders read the base color buffer from t0.
        Cmd.DiffuseSRV = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor);
    }
    else if (Pass == ERenderPass::FXAA && Context.SceneView)
    {
        // FXAA prepares SceneColor on t0 before submission; keep the command in sync
        // so SubmitCommand does not overwrite it with nullptr on a forced bind.
        Cmd.DiffuseSRV = Targets ? Targets->SceneColorCopySRV : nullptr;
    }

    Cmd.LightCB       = (Pass == ERenderPass::DeferredLighting && Context.Resources) ? &Context.Resources->GlobalLightBuffer : nullptr;
    Cmd.LocalLightSRV = (Pass == ERenderPass::DeferredLighting && Context.Resources) ? Context.Resources->LocalLightSRV : nullptr;
    Cmd.SortKey       = FDrawCommand::BuildSortKey(Pass, Shader, nullptr, Cmd.DiffuseSRV, ToPostProcessUserBits(PostProcessVariant));
}


void DrawCommandBuild::BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.SceneView)
    {
        return;
    }

    FLineBatch& EditorLines = Context.Renderer->GetEditorLineBatch();
    FLineBatch& GridLines   = Context.Renderer->GetGridLineBatch();
    EditorLines.Clear();
    GridLines.Clear();

    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;

    if (OverlayData && OverlayData->HasGrid())
    {
        GridLines.AddWorldHelpers(
            Context.SceneView->ShowFlags,
            OverlayData->GetGridSpacing(),
            OverlayData->GetGridHalfLineCount(),
            Context.SceneView->CameraPosition,
            Context.SceneView->CameraForward,
            Context.SceneView->bIsOrtho);
    }

    if (OverlayData)
    {
        for (const FSceneDebugLine& Line : OverlayData->GetDebugLines())
        {
            EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
        }
    }

    if (OverlayData)
    {
        for (const FSceneDebugAABB& Box : OverlayData->GetDebugAABBs())
        {
            const FVector& Min  = Box.Min;
            const FVector& Max  = Box.Max;
            const FVector  V[8] = {
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

    if (const FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Editor))
    {
        const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::EditorLines);

        auto AddBatch = [&](FLineBatch& Batch, const char* DebugName)
        {
            if (Batch.GetIndexCount() == 0 || !Batch.UploadBuffers(Context.Context))
            {
                return;
            }

            FDrawCommand& Cmd = OutList.AddCommand();
            Cmd.Shader        = const_cast<FGraphicsProgram*>(Shader);
            Cmd.DepthStencil  = State.DepthStencil;
            Cmd.Blend         = State.Blend;
            Cmd.Rasterizer    = ERasterizerState::SolidNoCull;
            Cmd.Topology      = State.Topology;
            Cmd.RawVB         = Batch.GetVBBuffer();
            Cmd.RawVBStride   = Batch.GetVBStride();
            Cmd.RawIB         = Batch.GetIBBuffer();
            Cmd.IndexCount    = Batch.GetIndexCount();
            Cmd.Pass          = ERenderPass::EditorLines;
            Cmd.DebugName     = DebugName;
            Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, nullptr);
        };

        AddBatch(GridLines, "GridLines");
        AddBatch(EditorLines, "DebugLines");
    }
}


void DrawCommandBuild::BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;
    if (!OverlayData)
    {
        return;
    }

    for (FPrimitiveProxy* Proxy : OverlayData->GetEditorHelperBillboards())
    {
        if (!Proxy)
        {
            continue;
        }

        BuildMeshDrawCommand(*Proxy, ERenderPass::OverlayBillboard, Context, OutList);
    }
}

void DrawCommandBuild::BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView)
    {
        return;
    }

    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;
    const FCollectedSceneData*   SceneData   = Context.Submission.SceneData;

    if (OverlayData)
    {
        for (FPrimitiveProxy* Proxy : OverlayData->GetEditorHelperTexts())
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

    if (!SceneData)
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

    for (const FSceneOverlayText& Text : SceneData->Primitives.OverlayTexts)
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

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);
    if (!Shader)
    {
        return;
    }

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::OverlayFont);
    FDrawCommand&                Cmd   = OutList.AddCommand();
    Cmd.Shader                         = Shader;
    Cmd.DepthStencil                   = State.DepthStencil;
    Cmd.Blend                          = State.Blend;
    Cmd.Rasterizer                     = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
    Cmd.Topology                       = State.Topology;
    Cmd.RawVB                          = FontBatch.GetScreenVBBuffer();
    Cmd.RawVBStride                    = FontBatch.GetScreenVBStride();
    Cmd.RawIB                          = FontBatch.GetScreenIBBuffer();
    Cmd.IndexCount                     = FontBatch.GetScreenIndexCount();
    Cmd.DiffuseSRV                     = FontRes->SRV;
    Cmd.Pass                           = ERenderPass::OverlayFont;
    Cmd.DebugName                      = "OverlayText";
    Cmd.SortKey                        = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}

void DrawCommandBuild::BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
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

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
    if (!Shader)
    {
        return;
    }

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::AlphaBlend);
    FDrawCommand&                Cmd   = OutList.AddCommand();
    Cmd.Shader                         = Shader;
    Cmd.DepthStencil                   = State.DepthStencil;
    Cmd.Blend                          = State.Blend;
    Cmd.Rasterizer                     = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
    Cmd.Topology                       = State.Topology;
    Cmd.RawVB                          = FontBatch.GetWorldVBBuffer();
    Cmd.RawVBStride                    = FontBatch.GetWorldVBStride();
    Cmd.RawIB                          = FontBatch.GetWorldIBBuffer();
    Cmd.FirstIndex                     = StartIndex;
    Cmd.IndexCount                     = EndIndex - StartIndex;
    Cmd.DiffuseSRV                     = FontRes->SRV;
    Cmd.Pass                           = ERenderPass::AlphaBlend;
    Cmd.DebugName                      = "WorldText";
    Cmd.SortKey                        = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}

void DrawCommandBuild::BuildOverlayWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
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

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
    if (!Shader)
    {
        return;
    }

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::OverlayTextWorld);
    FDrawCommand&                Cmd   = OutList.AddCommand();
    Cmd.Shader                         = Shader;
    Cmd.DepthStencil                   = State.DepthStencil;
    Cmd.Blend                          = State.Blend;
    Cmd.Rasterizer                     = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
    Cmd.Topology                       = State.Topology;
    Cmd.RawVB                          = FontBatch.GetOverlayWorldVBBuffer();
    Cmd.RawVBStride                    = FontBatch.GetOverlayWorldVBStride();
    Cmd.RawIB                          = FontBatch.GetOverlayWorldIBBuffer();
    Cmd.FirstIndex                     = StartIndex;
    Cmd.IndexCount                     = EndIndex - StartIndex;
    Cmd.DiffuseSRV                     = FontRes->SRV;
    Cmd.Pass                           = ERenderPass::OverlayTextWorld;
    Cmd.DebugName                      = "OverlayWorldText";
    Cmd.SortKey                        = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}


void DrawCommandBuild::BuildDecalDrawCommand(const FPrimitiveProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Proxy.DiffuseSRV || !Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return;
    }

    const FViewModePassDesc* Desc = Context.ViewMode.Registry->FindPassDesc(Context.ViewMode.ActiveViewMode, ERenderPass::Decal, ERenderShadingPath::Deferred);
    if (!Desc || !Desc->CompiledShader)
    {
        return;
    }

    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader        = Desc->CompiledShader;
    Cmd.DepthStencil  = EDepthStencilState::NoDepth;
    Cmd.Blend         = EBlendState::Opaque;
    Cmd.Rasterizer    = ERasterizerState::SolidNoCull;
    Cmd.Topology      = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Cmd.VertexCount   = 3;
    Cmd.DiffuseSRV    = Proxy.DiffuseSRV;
    Cmd.Pass          = ERenderPass::Decal;

    if (Proxy.ExtraCB.Buffer && Proxy.ExtraCB.Size > 0 && Context.Context)
    {
        Proxy.ExtraCB.Buffer->Update(Context.Context, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);

        if (Proxy.ExtraCB.Slot == ECBSlot::PerShader0)
        {
            Cmd.PerShaderCB[0] = Proxy.ExtraCB.Buffer;
        }
        else if (Proxy.ExtraCB.Slot == ECBSlot::PerShader1)
        {
            Cmd.PerShaderCB[1] = Proxy.ExtraCB.Buffer;
        }
    }

    Cmd.SortKey = FDrawCommand::BuildSortKey(ERenderPass::Decal, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}


