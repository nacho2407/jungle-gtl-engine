#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Context/ViewMode/ShadingModel.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/Resources/Shaders/ShaderVariantCache.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

namespace ViewModePassConfigUtils
{
void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value = "1");
}

enum class EViewModePostProcessVariant : uint16
{
    None        = 0,
    Outline     = 1,
    SceneDepth  = 2,
    WorldNormal = 3,
    LightHitMap = 4,
};

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant);

struct FViewModePassDesc
{
    ERenderPass        RenderPass;
    FShaderVariantDesc ShaderVariant;
    FGraphicsProgram*  CompiledShader  = nullptr;
    bool               bFullscreenPass = false;
};

// View-mode execution recipe: enabled pass families plus compiled shader variants.
struct FViewModePassConfig
{
    EViewMode     ViewMode;
    EShadingModel ShadingModel;

    bool bEnableDepthPre;
    bool bEnableOpaque;
    bool bEnableDecal;
    bool bEnableLighting;

    bool bEnableAdditiveDecal;
    bool bEnableAlphaBlend;

    bool bEnableNonLitViewMode;
    bool bEnableHeightFog;
    bool bEnableFXAA;

    EViewModePostProcessVariant PostProcessVariant;
    TArray<FViewModePassDesc>   Passes;
};

EShadingModel               GetViewModeShadingModel(const FViewModePassConfig* Config);
bool                        UsesViewModeDepthPre(const FViewModePassConfig* Config);
bool                        UsesViewModeOpaque(const FViewModePassConfig* Config);
bool                        UsesViewModeDecal(const FViewModePassConfig* Config);
bool                        UsesViewModeLighting(const FViewModePassConfig* Config);
bool                        UsesViewModeAdditiveDecal(const FViewModePassConfig* Config);
bool                        UsesViewModeAlphaBlend(const FViewModePassConfig* Config);
bool                        UsesNonLitViewMode(const FViewModePassConfig* Config);
bool                        UsesViewModeHeightFog(const FViewModePassConfig* Config);
bool                        UsesViewModeFXAA(const FViewModePassConfig* Config);
EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config);

FViewModePassDesc BuildViewModeOpaquePassDesc(EShadingModel ShadingModel);
FViewModePassDesc BuildViewModeDecalPassDesc(EShadingModel ShadingModel);
FViewModePassDesc BuildViewModeLightingPassDesc(EShadingModel ShadingModel);
void              BuildViewModePasses(FViewModePassConfig& Config);
void              InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache);

// Builds and caches pass/shader variant recipes for every supported editor view mode.
class FViewModePassRegistry
{
public:
    void Initialize(ID3D11Device* Device);
    void Release();

    bool                       HasConfig(EViewMode ViewMode) const;
    const FViewModePassConfig* GetConfig(EViewMode ViewMode) const;

    EShadingModel               GetShadingModel(EViewMode ViewMode) const;
    bool                        UsesDepthPrePass(EViewMode ViewMode) const;
    bool                        UsesOpaque(EViewMode ViewMode) const;
    bool                        UsesDecal(EViewMode ViewMode) const;
    bool                        UsesLightingPass(EViewMode ViewMode) const;
    bool                        UsesAdditiveDecal(EViewMode ViewMode) const;
    bool                        UsesAlphaBlend(EViewMode ViewMode) const;
    bool                        UsesNonLitViewMode(EViewMode ViewMode) const;
    bool                        UsesHeightFog(EViewMode ViewMode) const;
    bool                        UsesFXAA(EViewMode ViewMode) const;
    EViewModePostProcessVariant GetPostProcessVariant(EViewMode ViewMode) const;
    const FViewModePassDesc*    FindPassDesc(EViewMode ViewMode, ERenderPass RenderPass) const;

private:
    void RefreshCompiledShaders(FViewModePassConfig& Config) const;

    mutable FShaderVariantCache                  VariantCache;
    mutable TMap<EViewMode, FViewModePassConfig> Configs;
};
