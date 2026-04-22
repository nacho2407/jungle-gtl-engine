#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Passes/Base/PassRenderState.h"
#include "Render/Execute/Passes/Base/RenderPass.h"

class FDepthPrePass;
class FOpaquePass;
class FDecalPass;
class FLightingPass;
class FAdditiveDecalPass;
class FAlphaBlendPass;
class FHeightFogPass;
class FNonLitViewModePass;
class FFXAAPass;
class FPresentPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinePass;
class FGizmoPass;
class FOverlayBillboardPass;
class FOverlayTextPass;

/*
    ������Ʈ������ �����ϴ� �н� ��� �����Դϴ�.
    ���� ��ο� Ŀ�ǵ��� ERenderPass�ʹ� ������, ���������� �׷��� ��带 �ĺ��� �� ����մϴ�.
*/
enum class ERenderPassNodeType
{
    GridPass,
    DepthPrePass,
    LightCullingPass,
    OpaquePass,
    DecalPass,
    LightingPass,
    AdditiveDecalPass,
    AlphaBlendPass,
    NonLitViewModePass,
    HeightFogPass,
    FXAAPass,
    PresentPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinePass,
    OverlayBillboardPass,
    GizmoPass,
    OverlayTextPass,
	LightHitMapPass,
};

/*
    �н� ��� Ÿ�԰� ���� �н� ��ü, �н��� �⺻ ���� ǥ�� �Բ� �����ϴ� ������Ʈ���Դϴ�.
*/
class FRenderPassRegistry
{
public:
    FRenderPassRegistry() = default;
    ~FRenderPassRegistry();

    void Initialize();
    void Release();

    FRenderPass* FindPass(ERenderPassNodeType Type) const;
    const FPassRenderStateDesc& GetPassStateDesc(ERenderPass Pass) const;
    const FPassRenderStateDesc* GetPassStateDescs() const;

private:
    TMap<int32, FRenderPass*> Passes;
    FPassRenderStateDesc PassStateDescs[(uint32)ERenderPass::MAX] = {};
};
