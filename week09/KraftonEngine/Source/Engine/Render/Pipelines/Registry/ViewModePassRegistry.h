#pragma once

#include "Core/CoreTypes.h"

#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Passes/Scene/ShadingTypes.h"
#include "Render/Pipelines/Context/Scene/ViewTypes.h"
#include "Render/Resources/Shaders/ShaderVariantCache.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

/*
    뷰 모드별로 Scene 전체 패스 정책과 셰이더 변형을 정의하는 레지스트리입니다.
    Runner와 Collector는 이 레지스트리를 기준으로 어떤 패스를 실행/수집할지 결정합니다.
*/
namespace ViewModePassConfigUtils
{
void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value = "1");
}

enum class EViewModePostProcessVariant : uint16
{
    None = 0,
    Outline = 1,
    SceneDepth = 2,
    WorldNormal = 3,
};

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant);

struct FRenderPipelinePassDesc
{
    EPipelineStage Stage;
    ERenderPass RenderPass;
    FShaderVariantDesc ShaderVariant;
    FShader* CompiledShader = nullptr;
    bool bFullscreenPass = false;
};

struct FViewModePassConfig
{
    EViewMode ViewMode;
    EShadingModel ShadingModel;

    bool bEnableDepthPre;
    bool bEnableBaseDraw;
    bool bEnableDecal;
    bool bEnableLighting;

    bool bEnableAdditiveDecal;
    bool bEnableAlphaBlend;

    bool bEnableViewModeResolve;
    bool bEnableHeightFog;
    bool bEnableFXAA;

    EViewModePostProcessVariant PostProcessVariant;
    TArray<FRenderPipelinePassDesc> Passes;
};

const FRenderPipelinePassDesc* FindViewModePassDesc(const FViewModePassConfig* Config, EPipelineStage Stage);
EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config);
bool UsesViewModeDepthPre(const FViewModePassConfig* Config);
bool UsesViewModeBaseDraw(const FViewModePassConfig* Config);
bool UsesViewModeDecal(const FViewModePassConfig* Config);
bool UsesViewModeLighting(const FViewModePassConfig* Config);
bool UsesViewModeAdditiveDecal(const FViewModePassConfig* Config);
bool UsesViewModeAlphaBlend(const FViewModePassConfig* Config);
bool UsesViewModeResolve(const FViewModePassConfig* Config);
bool UsesViewModeHeightFog(const FViewModePassConfig* Config);
bool UsesViewModeFXAA(const FViewModePassConfig* Config);
EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config);

FRenderPipelinePassDesc BuildViewModeBaseDrawPassDesc(EShadingModel ShadingModel);
FRenderPipelinePassDesc BuildViewModeDecalPassDesc(EShadingModel ShadingModel);
FRenderPipelinePassDesc BuildViewModeLightingPassDesc(EShadingModel ShadingModel);
void BuildViewModePasses(FViewModePassConfig& Config);
void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache);

class FViewModePassRegistry
{
public:
    void Initialize(ID3D11Device* Device);
    void Release();

    bool HasConfig(EViewMode ViewMode) const;
    const FViewModePassConfig* GetConfig(EViewMode ViewMode) const;
    const FRenderPipelinePassDesc* FindPassDesc(EViewMode ViewMode, EPipelineStage Stage) const;

    EShadingModel GetShadingModel(EViewMode ViewMode) const;
    bool UsesDepthPrePass(EViewMode ViewMode) const;
    bool UsesBaseDraw(EViewMode ViewMode) const;
    bool UsesDecal(EViewMode ViewMode) const;
    bool UsesLightingPass(EViewMode ViewMode) const;
    bool UsesAdditiveDecal(EViewMode ViewMode) const;
    bool UsesAlphaBlend(EViewMode ViewMode) const;
    bool UsesViewModeResolve(EViewMode ViewMode) const;
    bool UsesHeightFog(EViewMode ViewMode) const;
    bool UsesFXAA(EViewMode ViewMode) const;
    EViewModePostProcessVariant GetPostProcessVariant(EViewMode ViewMode) const;

private:
    void RefreshCompiledShaders(FViewModePassConfig& Config) const;

    mutable FShaderVariantCache VariantCache;
    mutable TMap<int32, FViewModePassConfig> Configs;
};
