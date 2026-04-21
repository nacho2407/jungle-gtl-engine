#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Pipelines/ShadingTypes.h"
#include "Render/Resources/ShaderVariantCache.h"

/*
    뷰 모드별로 어떤 패스를 켜고, 어떤 셰이더 변형을 쓸지 정의하는 설정 모음입니다.
    Wireframe, SceneDepth, WorldNormal 같은 특수 모드도 이 헤더에서 한 번에 관리합니다.
*/

namespace ViewModePassConfigUtils
{
inline void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value = "1")
{
    Defines.push_back({ Name, Value });
}
} // namespace ViewModePassConfigUtils

enum class EViewModePostProcessVariant : uint16
{
    None = 0,
    Outline = 1,
    SceneDepth = 2,
    WorldNormal = 3,
};

inline uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant)
{
    return static_cast<uint16>(Variant);
}

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

    bool bEnableDepthPre = true;
    bool bEnableBaseDraw = true;
    bool bEnableDecal = true;
    bool bEnableLighting = false;

    bool bForceWireframeRasterizer = false;
    bool bSuppressSceneExtras = false;

    EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None;
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

inline bool UsesViewModeDepthPre(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableDepthPre : false;
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

inline EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config)
{
    return Config ? Config->PostProcessVariant : EViewModePostProcessVariant::None;
}

inline FRenderPipelinePassDesc BuildViewModeBaseDrawPassDesc(EShadingModel ShadingModel)
{
    FRenderPipelinePassDesc Pass;
    Pass.Stage = EPipelineStage::BaseDraw;
    Pass.RenderPass = ERenderPass::Opaque;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/BaseDraw.hlsl";
    Pass.ShaderVariant.VSEntry = "VS_BaseDraw";

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Gouraud";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_GOURAUD");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_GOURAUD_L");
        break;
    case EShadingModel::Lambert:
    case EShadingModel::WorldNormal:
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
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/DecalPass.hlsl";
    Pass.ShaderVariant.VSEntry = "VS_DecalFullscreen";
    Pass.bFullscreenPass = true;

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        Pass.ShaderVariant.PSEntry = "PS_Decal_Gouraud";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "DECAL_MODIFY_BASECOLOR");
        break;
    case EShadingModel::Lambert:
    case EShadingModel::WorldNormal:
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
    Pass.ShaderVariant.FilePath = "Shaders/ViewModes/UberLit.hlsl";
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
    Config = FViewModePassConfig();
    Config.ViewMode = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);

    switch (InViewMode)
    {
    case EViewMode::Wireframe:
        Config.bEnableDepthPre = false;
        Config.bEnableLighting = false;
        Config.bForceWireframeRasterizer = true;
        Config.bSuppressSceneExtras = true;
        break;

    case EViewMode::SceneDepth:
        Config.bEnableBaseDraw = false;
        Config.bEnableDecal = false;
        Config.bEnableLighting = false;
        Config.bSuppressSceneExtras = true;
        Config.PostProcessVariant = EViewModePostProcessVariant::SceneDepth;
        break;

    case EViewMode::WorldNormal:
        Config.ShadingModel = EShadingModel::WorldNormal;
        Config.bEnableLighting = false;
        Config.bSuppressSceneExtras = true;
        Config.PostProcessVariant = EViewModePostProcessVariant::WorldNormal;
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
            EViewMode::WorldNormal,
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

    bool UsesDepthPrePass(EViewMode ViewMode) const
    {
        return UsesViewModeDepthPre(GetConfig(ViewMode));
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

    EViewModePostProcessVariant GetPostProcessVariant(EViewMode ViewMode) const
    {
        return GetViewModePostProcessVariant(GetConfig(ViewMode));
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
