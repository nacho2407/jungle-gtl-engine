#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ShaderVariantCache.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ShadingTypes.h"

namespace ViewModePassConfigUtils
{
inline void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value = "1")
{
    Defines.push_back({ Name, Value });
}
} // namespace ViewModePassConfigUtils

struct FRenderPipelinePassDesc
{
    EPipelineStage Stage = EPipelineStage::BaseDraw;
    ERenderPass RenderPass = ERenderPass::Opaque;
    FShaderVariantDesc ShaderVariant;
    FShader* CompiledShader = nullptr;
    bool bFullscreenPass = false;
};

struct FViewModePassConfig
{
    EViewMode ViewMode = EViewMode::Lit_Phong;
    EShadingModel ShadingModel = EShadingModel::Gouraud;
    TArray<FRenderPipelinePassDesc> Passes;
};

inline const FRenderPipelinePassDesc* FindViewModePassDesc(const FViewModePassConfig* Config, EPipelineStage Stage)
{
    if (!Config)
    {
        return nullptr;
    }

    for (const FRenderPipelinePassDesc& Pass : Config->Passes)
    {
        if (Pass.Stage == Stage)
        {
            return &Pass;
        }
    }
    return nullptr;
}

inline EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config)
{
    return Config ? Config->ShadingModel : EShadingModel::Unlit;
}

inline FRenderPipelinePassDesc BuildViewModeBaseDrawPassDesc(EShadingModel ShadingModel)
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
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_GOURAUD");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_GOURAUD_L");
        break;
    case EShadingModel::Lambert:
        Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::BlinnPhong:
        Pass.ShaderVariant.PSEntry = "PS_BaseDraw_BlinnPhong";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_BLINNPHONG");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_MATERIAL_PARAM");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::Unlit:
    default:
        Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Unlit";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_UNLIT");
        break;
    }

    return Pass;
}

inline FRenderPipelinePassDesc BuildViewModeDecalPassDesc(EShadingModel ShadingModel)
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
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
        break;
    case EShadingModel::Lambert:
        Pass.ShaderVariant.PSEntry = "PS_Decal_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_NORMAL");
        break;
    case EShadingModel::BlinnPhong:
        Pass.ShaderVariant.PSEntry = "PS_Decal_BlinnPhong";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_MATERIAL_PARAM");
        break;
    case EShadingModel::Unlit:
    default:
        Pass.ShaderVariant.PSEntry = "PS_Decal_Unlit";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
        break;
    }

    return Pass;
}

inline FRenderPipelinePassDesc BuildViewModeLightingPassDesc(EShadingModel ShadingModel)
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
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_GOURAUD");
        break;
    case EShadingModel::Lambert:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_LAMBERT");
        break;
    case EShadingModel::BlinnPhong:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_PHONG");
        break;
    case EShadingModel::Unlit:
    default:
        break;
    }

    return Pass;
}

inline void BuildViewModePasses(FViewModePassConfig& Config)
{
    Config.Passes.clear();
    Config.Passes.push_back(BuildViewModeBaseDrawPassDesc(Config.ShadingModel));
    Config.Passes.push_back(BuildViewModeDecalPassDesc(Config.ShadingModel));

    if (Config.ShadingModel != EShadingModel::Unlit)
    {
        Config.Passes.push_back(BuildViewModeLightingPassDesc(Config.ShadingModel));
    }
}

inline void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache)
{
    Config.ViewMode = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);
    BuildViewModePasses(Config);

    for (FRenderPipelinePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    }
}

class FViewModePassRegistry
{
public:
    void Initialize(ID3D11Device* Device)
    {
        VariantCache.Initialize(Device);
        Configs.clear();

        const EViewMode Modes[] = {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_Phong,
            EViewMode::Unlit,
        };

        for (EViewMode Mode : Modes)
        {
            FViewModePassConfig Config;
            InitializeViewModePassConfig(Config, Mode, VariantCache);
            Configs.emplace(static_cast<int32>(Mode), std::move(Config));
        }
    }

    void Release()
    {
        Configs.clear();
        VariantCache.Release();
    }

    bool HasConfig(EViewMode ViewMode) const
    {
        return Configs.find(static_cast<int32>(ViewMode)) != Configs.end();
    }

    const FViewModePassConfig* GetConfig(EViewMode ViewMode) const
    {
        auto It = Configs.find(static_cast<int32>(ViewMode));
        return (It != Configs.end()) ? &It->second : nullptr;
    }

    const FRenderPipelinePassDesc* FindPassDesc(EViewMode ViewMode, EPipelineStage Stage) const
    {
        return FindViewModePassDesc(GetConfig(ViewMode), Stage);
    }

    EShadingModel GetShadingModel(EViewMode ViewMode) const
    {
        return GetViewModeShadingModel(GetConfig(ViewMode));
    }

private:
    FShaderVariantCache VariantCache;
    TMap<int32, FViewModePassConfig> Configs;
};