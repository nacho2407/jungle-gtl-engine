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
    bool bEnableBaseDraw = true;
    bool bEnableDecal = true;
    bool bEnableLighting = false;
    bool bForceWireframeRasterizer = false;
    bool bSuppressSceneExtras = false;
    uint16 PostProcessUserBits = 0;
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

inline bool UsesViewModeBaseDraw(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableBaseDraw : false;
}

inline bool UsesViewModeDecal(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableDecal : false;
}

inline bool UsesViewModeLighting(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableLighting : false;
}

inline bool ForcesViewModeWireframeRasterizer(const FViewModePassConfig* Config)
{
    return Config ? Config->bForceWireframeRasterizer : false;
}

inline bool SuppressesViewModeSceneExtras(const FViewModePassConfig* Config)
{
    return Config ? Config->bSuppressSceneExtras : false;
}

inline uint16 GetViewModePostProcessUserBits(const FViewModePassConfig* Config)
{
    return Config ? Config->PostProcessUserBits : 0;
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
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_DIRECT_FINAL_OUTPUT");
        break;
    }

    return Pass;
}

inline FRenderPipelinePassDesc BuildViewModeLightingPassDesc(EShadingModel ShadingModel)
{
    FRenderPipelinePassDesc Pass;
    Pass.Stage = EPipelineStage::Lighting;
    Pass.RenderPass = ERenderPass::Lighting;
    Pass.ShaderVariant.FilePath = "Shaders/LightingPass.hlsl";
    Pass.ShaderVariant.VSEntry = "VS_Fullscreen";
    Pass.bFullscreenPass = true;

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        Pass.ShaderVariant.PSEntry = "PS_Lighting_Gouraud";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_GOURAUD");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_GOURAUD_L");
        break;
    case EShadingModel::Lambert:
        Pass.ShaderVariant.PSEntry = "PS_Lighting_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL");
        break;
    case EShadingModel::BlinnPhong:
        Pass.ShaderVariant.PSEntry = "PS_Lighting_BlinnPhong";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_PHONG");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_MATERIAL_PARAM");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_SPECULAR");
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
    if (Config.bEnableBaseDraw)
    {
        Config.Passes.push_back(BuildViewModeBaseDrawPassDesc(Config.ShadingModel));
    }

    if (Config.bEnableDecal)
    {
        Config.Passes.push_back(BuildViewModeDecalPassDesc(Config.ShadingModel));
    }

    if (Config.bEnableLighting)
    {
        Config.Passes.push_back(BuildViewModeLightingPassDesc(Config.ShadingModel));
    }
}

inline void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache)
{
    Config.ViewMode = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);

    switch (InViewMode)
    {
    case EViewMode::Wireframe:
        Config.bEnableLighting = false;
        Config.bForceWireframeRasterizer = true;
        Config.bSuppressSceneExtras = true;
        break;
    case EViewMode::SceneDepth:
        Config.bEnableBaseDraw = false;
        Config.bEnableDecal = false;
        Config.bEnableLighting = false;
        Config.bSuppressSceneExtras = true;
        Config.PostProcessUserBits = 2;
        break;
    case EViewMode::Normal:
        Config.bEnableLighting = false;
        Config.bSuppressSceneExtras = true;
        Config.PostProcessUserBits = 3;
        break;
    case EViewMode::Unlit:
        Config.bEnableLighting = false;
        break;
    default:
        Config.bEnableLighting = IsLitShadingModel(Config.ShadingModel);
        break;
    }

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
            EViewMode::Wireframe,
            EViewMode::SceneDepth,
            EViewMode::Normal,
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
        if (It == Configs.end())
        {
            return nullptr;
        }

        RefreshCompiledShaders(It->second);
        return &It->second;
    }

    const FRenderPipelinePassDesc* FindPassDesc(EViewMode ViewMode, EPipelineStage Stage) const
    {
        return FindViewModePassDesc(GetConfig(ViewMode), Stage);
    }

    EShadingModel GetShadingModel(EViewMode ViewMode) const
    {
        return GetViewModeShadingModel(GetConfig(ViewMode));
    }

    bool UsesBaseDraw(EViewMode ViewMode) const
    {
        return UsesViewModeBaseDraw(GetConfig(ViewMode));
    }

    bool UsesDecal(EViewMode ViewMode) const
    {
        return UsesViewModeDecal(GetConfig(ViewMode));
    }

    bool UsesLightingPass(EViewMode ViewMode) const
    {
        return UsesViewModeLighting(GetConfig(ViewMode));
    }

    bool ShouldForceWireframeRasterizer(EViewMode ViewMode) const
    {
        return ForcesViewModeWireframeRasterizer(GetConfig(ViewMode));
    }

    bool SuppressesSceneExtras(EViewMode ViewMode) const
    {
        return SuppressesViewModeSceneExtras(GetConfig(ViewMode));
    }

    uint16 GetPostProcessUserBits(EViewMode ViewMode) const
    {
        return GetViewModePostProcessUserBits(GetConfig(ViewMode));
    }

private:
    void RefreshCompiledShaders(FViewModePassConfig& Config) const
    {
        for (FRenderPipelinePassDesc& Pass : Config.Passes)
        {
            Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
        }
    }

    mutable FShaderVariantCache VariantCache;
    mutable TMap<int32, FViewModePassConfig> Configs;
};
