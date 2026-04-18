#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ShaderVariantCache.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ShadingTypes.h"

namespace ViewModePipeline
{
inline void AddDefine(TArray<FShaderMacroDefine> &Defines, const char *Name, const char *Value = "1")
{
    Defines.push_back({Name, Value});
}
} // namespace ViewModePipeline

struct FRenderPipelinePassDesc
{
    EPipelineStage Stage = EPipelineStage::BaseDraw;
    ERenderPass RenderPass = ERenderPass::Opaque;
    FShaderVariantDesc ShaderVariant;
    FShader *CompiledShader = nullptr;
    bool bFullscreenPass = false;
};

class FViewModeRenderPipeline
{
  public:
    void Initialize(ID3D11Device *Device, EViewMode InViewMode, FShaderVariantCache &VariantCache)
    {
        (void)Device;

        ViewMode = InViewMode;
        ShadingModel = GetShadingModelFromViewMode(InViewMode);
        BuildPasses();

        for (FRenderPipelinePassDesc &Pass : Passes)
        {
            Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
        }
    }

    EViewMode GetViewMode() const
    {
        return ViewMode;
    }
    EShadingModel GetShadingModel() const
    {
        return ShadingModel;
    }
    bool HasLightingPass() const
    {
        return true;
    }

    const FRenderPipelinePassDesc *FindPass(EPipelineStage Stage) const
    {
        for (const FRenderPipelinePassDesc &Pass : Passes)
        {
            if (Pass.Stage == Stage)
            {
                return &Pass;
            }
        }
        return nullptr;
    }

  private:
    void BuildPasses()
    {
        Passes.clear();
        Passes.push_back(BuildBaseDrawPass());
        Passes.push_back(BuildDecalPass());
        Passes.push_back(BuildLightingPass());
    }

    FRenderPipelinePassDesc BuildBaseDrawPass() const
    {
        FRenderPipelinePassDesc Pass;
        Pass.Stage = EPipelineStage::BaseDraw;
        Pass.RenderPass = ERenderPass::Opaque;
        Pass.ShaderVariant.FilePath = "Shaders/BaseDraw.hlsl";
        Pass.ShaderVariant.VSEntry = "VS_BaseDraw";

        switch (ShadingModel)
        {
        case EShadingModel::Gouraud:
            Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Gouraud";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_GOURAUD");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_GOURAUD_L");
            break;
        case EShadingModel::Lambert:
            Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Lambert";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_LAMBERT");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
            break;
        case EShadingModel::BlinnPhong:
            Pass.ShaderVariant.PSEntry = "PS_BaseDraw_BlinnPhong";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_BLINNPHONG");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_MATERIAL_PARAM");
            break;
        case EShadingModel::Unlit:
        default:
            Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Unlit";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_UNLIT");
            break;
        }

        return Pass;
    }

    FRenderPipelinePassDesc BuildDecalPass() const
    {
        FRenderPipelinePassDesc Pass;
        Pass.Stage = EPipelineStage::Decal;
        Pass.RenderPass = ERenderPass::Decal;
        Pass.ShaderVariant.FilePath = "Shaders/DecalPass.hlsl";
        Pass.ShaderVariant.VSEntry = "VS_DecalFullscreen";
        Pass.bFullscreenPass = true;

        switch (ShadingModel)
        {
        case EShadingModel::Gouraud:
            Pass.ShaderVariant.PSEntry = "PS_Decal_Gouraud";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
            break;
        case EShadingModel::Lambert:
            Pass.ShaderVariant.PSEntry = "PS_Decal_Lambert";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_NORMAL");
            break;
        case EShadingModel::BlinnPhong:
            Pass.ShaderVariant.PSEntry = "PS_Decal_BlinnPhong";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_NORMAL");
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_MATERIAL_PARAM");
            break;
        case EShadingModel::Unlit:
        default:
            Pass.ShaderVariant.PSEntry = "PS_Decal_Unlit";
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
            break;
        }

        return Pass;
    }

    FRenderPipelinePassDesc BuildLightingPass() const
    {
        FRenderPipelinePassDesc Pass;
        Pass.Stage = EPipelineStage::Lighting;
        Pass.RenderPass = ERenderPass::Lighting;
        Pass.ShaderVariant.FilePath = "Shaders/UberLit.hlsl";
        Pass.ShaderVariant.VSEntry = "VS_Fullscreen";
        Pass.ShaderVariant.PSEntry = "PS_UberLit";
        Pass.bFullscreenPass = true;

        switch (ShadingModel)
        {
        case EShadingModel::Gouraud:
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_GOURAUD");
            break;
        case EShadingModel::Lambert:
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_LAMBERT");
            break;
        case EShadingModel::BlinnPhong:
            ViewModePipeline::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_PHONG");
            break;
        case EShadingModel::Unlit:
        default:
            break;
        }

        return Pass;
    }

  private:
    EViewMode ViewMode = EViewMode::Lit_Phong;
    EShadingModel ShadingModel = EShadingModel::Gouraud;
    TArray<FRenderPipelinePassDesc> Passes;
};

class FViewModeRenderPipelineLibrary
{
  public:
    void Initialize(ID3D11Device *Device)
    {
        VariantCache.Initialize(Device);
        Pipelines.clear();

        const EViewMode Modes[] = {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_Phong,
            EViewMode::Unlit,
        };

        for (EViewMode Mode : Modes)
        {
            FViewModeRenderPipeline Pipeline;
            Pipeline.Initialize(Device, Mode, VariantCache);
            Pipelines.emplace(static_cast<int32>(Mode), std::move(Pipeline));
        }
    }

    void Release()
    {
        Pipelines.clear();
        VariantCache.Release();
    }

    const FViewModeRenderPipeline *Get(EViewMode ViewMode) const
    {
        auto It = Pipelines.find(static_cast<int32>(ViewMode));
        return (It != Pipelines.end()) ? &It->second : nullptr;
    }

  private:
    FShaderVariantCache VariantCache;
    TMap<int32, FViewModeRenderPipeline> Pipelines;
};
