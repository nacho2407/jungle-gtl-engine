#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/Execute/Passes/Scene/ShadingTypes.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/Resources/Shaders/ShaderVariantCache.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

/*
    �� ��庰�� Scene ��ü �н� ��å�� ���̴� ������ �����ϴ� ������Ʈ���Դϴ�.
    Runner�� Collector�� �� ������Ʈ���� �������� � �н��� ����/�������� �����մϴ�.
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
	LightHitMap = 4,
};

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant);

struct FRenderPipelinePassDesc
{
    EViewModeStage Stage;
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
    bool bEnableOpaque;
    bool bEnableDecal;
    bool bEnableLighting;

    bool bEnableAdditiveDecal;
    bool bEnableAlphaBlend;

    bool bEnableNonLitViewMode;
    bool bEnableHeightFog;
    bool bEnableFXAA;

    EViewModePostProcessVariant PostProcessVariant;
    TArray<FRenderPipelinePassDesc> Passes;
};

const FRenderPipelinePassDesc* FindViewModePassDesc(const FViewModePassConfig* Config, EViewModeStage Stage);
EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config);
bool UsesViewModeDepthPre(const FViewModePassConfig* Config);
bool UsesViewModeOpaque(const FViewModePassConfig* Config);
bool UsesViewModeDecal(const FViewModePassConfig* Config);
bool UsesViewModeLighting(const FViewModePassConfig* Config);
bool UsesViewModeAdditiveDecal(const FViewModePassConfig* Config);
bool UsesViewModeAlphaBlend(const FViewModePassConfig* Config);
bool UsesNonLitViewMode(const FViewModePassConfig* Config);
bool UsesViewModeHeightFog(const FViewModePassConfig* Config);
bool UsesViewModeFXAA(const FViewModePassConfig* Config);
EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config);

FRenderPipelinePassDesc BuildViewModeOpaquePassDesc(EShadingModel ShadingModel);
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
    const FRenderPipelinePassDesc* FindPassDesc(EViewMode ViewMode, EViewModeStage Stage) const;

    EShadingModel GetShadingModel(EViewMode ViewMode) const;
    bool UsesDepthPrePass(EViewMode ViewMode) const;
    bool UsesOpaque(EViewMode ViewMode) const;
    bool UsesDecal(EViewMode ViewMode) const;
    bool UsesLightingPass(EViewMode ViewMode) const;
    bool UsesAdditiveDecal(EViewMode ViewMode) const;
    bool UsesAlphaBlend(EViewMode ViewMode) const;
    bool UsesNonLitViewMode(EViewMode ViewMode) const;
    bool UsesHeightFog(EViewMode ViewMode) const;
    bool UsesFXAA(EViewMode ViewMode) const;
    EViewModePostProcessVariant GetPostProcessVariant(EViewMode ViewMode) const;

private:
    void RefreshCompiledShaders(FViewModePassConfig& Config) const;

    mutable FShaderVariantCache VariantCache;
    mutable TMap<int32, FViewModePassConfig> Configs;
};
