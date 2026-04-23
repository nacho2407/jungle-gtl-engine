#pragma once

#include "Core/EngineTypes.h"
#include "Core/CoreTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/ConstantBufferBinding.h"
#include "Render/Scene/Proxies/Primitive/MeshSectionRenderData.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Render/Resources/State/RenderStateTypes.h"

class UPrimitiveComponent;
class FGraphicsProgram;
class FMeshBuffer;
class FScene;
struct FSceneView;

// ============================================================
// ============================================================
class FPrimitiveSceneProxy : public FSceneProxy
{
public:
    FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
    virtual ~FPrimitiveSceneProxy() = default;

    virtual void UpdateTransform();
    virtual void UpdateMaterial();
    virtual void UpdateVisibility();
    virtual void UpdateMesh();

    UPrimitiveComponent* Owner = nullptr;

    // --- LOD ---
    FVector      CachedWorldPos;
    uint32       CurrentLOD = 0;
    virtual void UpdateLOD(uint32 /*LODLevel*/) {}

    virtual void UpdatePerViewport(const FSceneView& SceneView) {}

    void CollectSelectedVisuals(FScene& Scene) const;

    bool bVisible         = true;
    bool bSelected        = false;
    bool bSupportsOutline = true;
    bool bNeverCull       = false;
    bool bShowAABB        = true;

    ERenderPass Pass = ERenderPass::Opaque;

    EBlendState        Blend        = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState   Rasterizer   = ERasterizerState::SolidBackCull;

    FGraphicsProgram* Shader             = nullptr;
    FMeshBuffer*      MeshBuffer         = nullptr;
    FPerObjectCBData  PerObjectConstants = {};
    FBoundingBox      CachedBounds;
    mutable bool      bPerObjectCBDirty = true;

    TArray<FMeshSectionRenderData> SectionRenderData;

    FCBBindingEntry ExtraCB;

    ID3D11ShaderResourceView* DiffuseSRV    = nullptr;
    ID3D11ShaderResourceView* NormalSRV     = nullptr;
    ID3D11ShaderResourceView* SpecularSRV   = nullptr;
    FConstantBuffer*          MaterialCB[2] = {};

    bool bPerViewportUpdate           = false;
    bool bFontBatched                 = false;
    bool bAllowViewModeShaderOverride = false;

    uint32 LastLODUpdateFrame = UINT32_MAX;

    void MarkPerObjectCBDirty() const { bPerObjectCBDirty = true; }
    void ClearPerObjectCBDirty() const { bPerObjectCBDirty = false; }
    bool NeedsPerObjectCBUpload() const { return bPerObjectCBDirty; }
};
