#include "Render/Pipelines/Registry/ViewModePassRegistry.h"

namespace ViewModePassConfigUtils
{
void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value)
{
    Defines.push_back({ Name, Value });
}
} // namespace ViewModePassConfigUtils

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant)
{
    return static_cast<uint16>(Variant);
}

const FRenderPipelinePassDesc* FindViewModePassDesc(const FViewModePassConfig* Config, EPipelineStage Stage)
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

EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config) { return Config ? Config->ShadingModel : EShadingModel::Unlit; }
bool UsesViewModeDepthPre(const FViewModePassConfig* Config) { return Config ? Config->bEnableDepthPre : false; }
bool UsesViewModeBaseDraw(const FViewModePassConfig* Config) { return Config ? Config->bEnableBaseDraw : false; }
bool UsesViewModeDecal(const FViewModePassConfig* Config) { return Config ? Config->bEnableDecal : false; }
bool UsesViewModeLighting(const FViewModePassConfig* Config) { return Config ? Config->bEnableLighting : false; }
bool UsesViewModeAdditiveDecal(const FViewModePassConfig* Config) { return Config ? Config->bEnableAdditiveDecal : false; }
bool UsesViewModeAlphaBlend(const FViewModePassConfig* Config) { return Config ? Config->bEnableAlphaBlend : false; }
bool UsesViewModeResolve(const FViewModePassConfig* Config) { return Config ? Config->bEnableViewModeResolve : false; }
bool UsesViewModeHeightFog(const FViewModePassConfig* Config) { return Config ? Config->bEnableHeightFog : false; }
bool UsesViewModeFXAA(const FViewModePassConfig* Config) { return Config ? Config->bEnableFXAA : false; }
EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config) { return Config ? Config->PostProcessVariant : EViewModePostProcessVariant::None; }

FRenderPipelinePassDesc BuildViewModeBaseDrawPassDesc(EShadingModel ShadingModel)
{
    FRenderPipelinePassDesc Pass = {};
    Pass.Stage = EPipelineStage::BaseDraw;
    Pass.RenderPass = ERenderPass::Opaque;
    Pass.bFullscreenPass = false;
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
    case EShadingModel::WorldNormal:
        Pass.ShaderVariant.PSEntry = "PS_BaseDraw_Lambert";
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "SHADING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "OUTPUT_NORMAL");
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

FRenderPipelinePassDesc BuildViewModeDecalPassDesc(EShadingModel ShadingModel)
{
    FRenderPipelinePassDesc Pass = {};
    Pass.Stage = EPipelineStage::Decal;
    Pass.RenderPass = ERenderPass::Decal;
    Pass.bFullscreenPass = true;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/DecalPass.hlsl";
    Pass.ShaderVariant.VSEntry = "VS_DecalFullscreen";

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

FRenderPipelinePassDesc BuildViewModeLightingPassDesc(EShadingModel ShadingModel)
{
    FRenderPipelinePassDesc Pass = {};
    Pass.Stage = EPipelineStage::Lighting;
    Pass.RenderPass = ERenderPass::Lighting;
    Pass.bFullscreenPass = true;
    Pass.ShaderVariant.FilePath = "Shaders/ViewModes/UberLit.hlsl";
    Pass.ShaderVariant.VSEntry = "VS_Fullscreen";
    Pass.ShaderVariant.PSEntry = "PS_UberLit";

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

void BuildViewModePasses(FViewModePassConfig& Config)
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

void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache)
{
    Config = {};
    Config.ViewMode = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);

    switch (InViewMode)
    {
    case EViewMode::Wireframe:
        Config.bEnableDepthPre = false;
        Config.bEnableBaseDraw = true;
        Config.bEnableDecal = false;
        Config.bEnableLighting = false;
        Config.bEnableAdditiveDecal = false;
        Config.bEnableAlphaBlend = false;
        Config.bEnableViewModeResolve = false;
        Config.bEnableHeightFog = false;
        Config.bEnableFXAA = false;
        Config.PostProcessVariant = EViewModePostProcessVariant::None;
        break;

    case EViewMode::SceneDepth:
        Config.bEnableDepthPre = true;
        Config.bEnableBaseDraw = false;
        Config.bEnableDecal = false;
        Config.bEnableLighting = false;
        Config.bEnableAdditiveDecal = false;
        Config.bEnableAlphaBlend = false;
        Config.bEnableViewModeResolve = true;
        Config.bEnableHeightFog = false;
        Config.bEnableFXAA = false;
        Config.PostProcessVariant = EViewModePostProcessVariant::SceneDepth;
        break;

    case EViewMode::WorldNormal:
        Config.bEnableDepthPre = true;
        Config.bEnableBaseDraw = true;
        Config.bEnableDecal = true;
        Config.bEnableLighting = false;
        Config.bEnableAdditiveDecal = false;
        Config.bEnableAlphaBlend = false;
        Config.bEnableViewModeResolve = true;
        Config.bEnableHeightFog = false;
        Config.bEnableFXAA = false;
        Config.PostProcessVariant = EViewModePostProcessVariant::WorldNormal;
        break;

    case EViewMode::Unlit:
        Config.bEnableDepthPre = true;
        Config.bEnableBaseDraw = true;
        Config.bEnableDecal = true;
        Config.bEnableLighting = false;
        Config.bEnableAdditiveDecal = true;
        Config.bEnableAlphaBlend = true;
        Config.bEnableViewModeResolve = false;
        Config.bEnableHeightFog = true;
        Config.bEnableFXAA = true;
        Config.PostProcessVariant = EViewModePostProcessVariant::None;
        break;

    case EViewMode::Lit_Gouraud:
    case EViewMode::Lit_Lambert:
    case EViewMode::Lit_Phong:
    default:
        Config.bEnableDepthPre = true;
        Config.bEnableBaseDraw = true;
        Config.bEnableDecal = true;
        Config.bEnableLighting = true;
        Config.bEnableAdditiveDecal = true;
        Config.bEnableAlphaBlend = true;
        Config.bEnableViewModeResolve = false;
        Config.bEnableHeightFog = true;
        Config.bEnableFXAA = true;
        Config.PostProcessVariant = EViewModePostProcessVariant::None;
        break;
    }

    BuildViewModePasses(Config);

    for (FRenderPipelinePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    }
}

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
        Configs.emplace(static_cast<int32>(Mode), std::move(Config));
    }
}

void FViewModePassRegistry::Release()
{
    Configs.clear();
    VariantCache.Release();
}

bool FViewModePassRegistry::HasConfig(EViewMode ViewMode) const
{
    return Configs.find(static_cast<int32>(ViewMode)) != Configs.end();
}

const FViewModePassConfig* FViewModePassRegistry::GetConfig(EViewMode ViewMode) const
{
    auto It = Configs.find(static_cast<int32>(ViewMode));
    if (It == Configs.end())
    {
        return nullptr;
    }

    RefreshCompiledShaders(It->second);
    return &It->second;
}

const FRenderPipelinePassDesc* FViewModePassRegistry::FindPassDesc(EViewMode ViewMode, EPipelineStage Stage) const { return FindViewModePassDesc(GetConfig(ViewMode), Stage); }
EShadingModel FViewModePassRegistry::GetShadingModel(EViewMode ViewMode) const { return GetViewModeShadingModel(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesDepthPrePass(EViewMode ViewMode) const { return UsesViewModeDepthPre(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesBaseDraw(EViewMode ViewMode) const { return UsesViewModeBaseDraw(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesDecal(EViewMode ViewMode) const { return UsesViewModeDecal(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesLightingPass(EViewMode ViewMode) const { return UsesViewModeLighting(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesAdditiveDecal(EViewMode ViewMode) const { return UsesViewModeAdditiveDecal(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesAlphaBlend(EViewMode ViewMode) const { return UsesViewModeAlphaBlend(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesViewModeResolve(EViewMode ViewMode) const { return ::UsesViewModeResolve(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesHeightFog(EViewMode ViewMode) const { return UsesViewModeHeightFog(GetConfig(ViewMode)); }
bool FViewModePassRegistry::UsesFXAA(EViewMode ViewMode) const { return UsesViewModeFXAA(GetConfig(ViewMode)); }
EViewModePostProcessVariant FViewModePassRegistry::GetPostProcessVariant(EViewMode ViewMode) const { return GetViewModePostProcessVariant(GetConfig(ViewMode)); }

void FViewModePassRegistry::RefreshCompiledShaders(FViewModePassConfig& Config) const
{
    for (FRenderPipelinePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    }
}
