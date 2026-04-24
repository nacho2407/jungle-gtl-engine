// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/ViewModePassRegistry.h"

namespace ViewModePassConfigUtils
{
// ========== Define Helpers ==========

void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value)
{
    Defines.push_back({ Name, Value });
}

} // namespace ViewModePassConfigUtils
  // namespace ViewModePassConfigUtils

// ========== Config Queries ==========

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant)
{
    return static_cast<uint16>(Variant);
}


EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config)
{
    return Config ? Config->ShadingModel : EShadingModel::Unlit;
}

bool UsesViewModeDepthPre(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableDepthPre : false;
}

bool UsesViewModeOpaque(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableOpaque : false;
}

bool UsesViewModeDecal(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableDecal : false;
}

bool UsesViewModeLighting(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableLighting : false;
}

bool UsesViewModeAdditiveDecal(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableAdditiveDecal : false;
}

bool UsesViewModeAlphaBlend(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableAlphaBlend : false;
}

bool UsesNonLitViewMode(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableNonLitViewMode : false;
}

bool UsesViewModeHeightFog(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableHeightFog : false;
}

bool UsesViewModeFXAA(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableFXAA : false;
}

EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config)
{
    return Config ? Config->PostProcessVariant : EViewModePostProcessVariant::None;
}


// ========== Pass Descs ==========

FViewModePassDesc BuildViewModeDeferredOpaquePassDesc(EShadingModel ShadingModel)
{
    FViewModePassDesc Pass      = {};
    Pass.RenderPass             = ERenderPass::Opaque;
    Pass.bFullscreenPass        = false;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/DeferredOpaquePass.hlsl";
    Pass.ShaderVariant.VSEntry  = "VS_DeferredOpaque";

    // ---------- Shading Permutation ----------
    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        Pass.ShaderVariant.PSEntry = "PS_Opaque_Gouraud";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_GOURAUD");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_GOURAUD_L");
        break;
    case EShadingModel::Lambert:
        Pass.ShaderVariant.PSEntry = "PS_Opaque_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::BlinnPhong:
        Pass.ShaderVariant.PSEntry = "PS_Opaque_BlinnPhong";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_BLINNPHONG");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_MATERIAL_PARAM");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::WorldNormal:
        Pass.ShaderVariant.PSEntry = "PS_Opaque_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::Unlit:
    default:
        Pass.ShaderVariant.PSEntry = "PS_Opaque_Unlit";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_UNLIT");
        break;
    }


    return Pass;
}


FViewModePassDesc BuildViewModeDeferredDecalPassDesc(EShadingModel ShadingModel)
{
    FViewModePassDesc Pass      = {};
    Pass.RenderPass             = ERenderPass::Decal;
    Pass.bFullscreenPass        = true;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/DeferredDecalPass.hlsl";
    Pass.ShaderVariant.VSEntry  = "VS_DeferredDecalFullscreen";

    // ---------- Decal Surface Writes ----------
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


FViewModePassDesc BuildViewModeDeferredLightingPassDesc(EShadingModel ShadingModel)
{
    FViewModePassDesc Pass      = {};
    Pass.RenderPass             = ERenderPass::DeferredLighting;
    Pass.bFullscreenPass        = true;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/DeferredLightingPass.hlsl";
    Pass.ShaderVariant.VSEntry  = "VS_Fullscreen";
    Pass.ShaderVariant.PSEntry  = "PS_UberLit";

    // ---------- Lighting Permutation ----------
    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_GOURAUD");
        break;
    case EShadingModel::Lambert:
    case EShadingModel::WorldNormal:
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


// ========== View Mode Config Build ==========

void BuildViewModePasses(FViewModePassConfig& Config)
{
    Config.Passes.clear();

    if (Config.bEnableOpaque)
    {
        Config.Passes.push_back(BuildViewModeDeferredOpaquePassDesc(Config.ShadingModel));
    }


    if (Config.bEnableDecal)
    {
        Config.Passes.push_back(BuildViewModeDeferredDecalPassDesc(Config.ShadingModel));
    }


    if (Config.bEnableLighting)
    {
        Config.Passes.push_back(BuildViewModeDeferredLightingPassDesc(Config.ShadingModel));
    }
}


void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache)
{
    Config              = {};
    Config.ViewMode     = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);

    // ---------- Pipeline Flags ----------
    switch (InViewMode)
    {
    case EViewMode::Wireframe:
        Config.bEnableDepthPre       = false;
        Config.bEnableOpaque         = true;
        Config.bEnableDecal          = false;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;

    case EViewMode::SceneDepth:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = false;
        Config.bEnableDecal          = false;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = true;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::SceneDepth;
        break;

    case EViewMode::WorldNormal:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableDecal          = true;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = true;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::WorldNormal;
        break;

    case EViewMode::Unlit:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableDecal          = true;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = true;
        Config.bEnableFXAA           = true;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;

    case EViewMode::Lit_Gouraud:
    case EViewMode::Lit_Lambert:
    case EViewMode::Lit_Phong:
    default:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableDecal          = true;
        Config.bEnableLighting       = true;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = true;
        Config.bEnableFXAA           = true;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;
    }


    BuildViewModePasses(Config);

    // ---------- Initial Shader Compile ----------
    for (FViewModePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    }
}


// ========== Registry Lifecycle ==========

void FViewModePassRegistry::Initialize(ID3D11Device* Device)
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
        FViewModePassConfig Config = {};
        InitializeViewModePassConfig(Config, Mode, VariantCache);
        Configs.emplace(Mode, std::move(Config));
    }
}


void FViewModePassRegistry::Release()
{
    Configs.clear();
    VariantCache.Release();
}


// ========== Registry Lookup ==========

bool FViewModePassRegistry::HasConfig(EViewMode ViewMode) const
{
    return Configs.find(ViewMode) != Configs.end();
}


const FViewModePassConfig* FViewModePassRegistry::GetConfig(EViewMode ViewMode) const
{
    auto It = Configs.find(ViewMode);
    if (It == Configs.end())
    {
        return nullptr;
    }


    RefreshCompiledShaders(It->second);
    return &It->second;
}

EShadingModel FViewModePassRegistry::GetShadingModel(EViewMode ViewMode) const
{
    return GetViewModeShadingModel(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesDepthPrePass(EViewMode ViewMode) const
{
    return UsesViewModeDepthPre(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesOpaque(EViewMode ViewMode) const
{
    return UsesViewModeOpaque(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesDecal(EViewMode ViewMode) const
{
    return UsesViewModeDecal(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesLightingPass(EViewMode ViewMode) const
{
    return UsesViewModeLighting(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesAdditiveDecal(EViewMode ViewMode) const
{
    return UsesViewModeAdditiveDecal(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesAlphaBlend(EViewMode ViewMode) const
{
    return UsesViewModeAlphaBlend(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesNonLitViewMode(EViewMode ViewMode) const
{
    return ::UsesNonLitViewMode(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesHeightFog(EViewMode ViewMode) const
{
    return UsesViewModeHeightFog(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesFXAA(EViewMode ViewMode) const
{
    return UsesViewModeFXAA(GetConfig(ViewMode));
}

EViewModePostProcessVariant FViewModePassRegistry::GetPostProcessVariant(EViewMode ViewMode) const
{
    return GetViewModePostProcessVariant(GetConfig(ViewMode));
}


void FViewModePassRegistry::RefreshCompiledShaders(FViewModePassConfig& Config) const
{
    for (FViewModePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    }
}

const FViewModePassDesc* FViewModePassRegistry::FindPassDesc(EViewMode ViewMode, ERenderPass RenderPass) const
{
    const FViewModePassConfig* Config = GetConfig(ViewMode);
    if (!Config)
    {
        return nullptr;
    }

    for (const FViewModePassDesc& Pass : Config->Passes)
    {
        if (Pass.RenderPass == RenderPass)
        {
            return &Pass;
        }
    }

    return nullptr;
}
