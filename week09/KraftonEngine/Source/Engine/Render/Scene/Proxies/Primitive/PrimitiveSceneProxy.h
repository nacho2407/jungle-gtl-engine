#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Render/Resources/RenderResources.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

class UPrimitiveComponent;
class FShader;
class FMeshBuffer;
class FScene;
struct FSceneView;

// ============================================================
// FPrimitiveSceneProxy ? UPrimitiveComponent�� ���� ������ �̷� (�⺻ Ŭ����)
// ============================================================
// ������Ʈ ��� �� CreateSceneProxy()�� 1ȸ ����.
// ���� DirtyFlags�� ���� �ʵ常 ���� �Լ��� ���� ����.
// Renderer�� �� ������ �� ���Ͻø� ���� ��ȸ�Ͽ� draw call ����.
class FPrimitiveSceneProxy : public FSceneProxy
{
public:
    FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
    virtual ~FPrimitiveSceneProxy() = default;

    // --- ���� ���� �������̽� (����Ŭ������ �������̵�) ---
    virtual void UpdateTransform();
    virtual void UpdateMaterial();
    virtual void UpdateVisibility();
    virtual void UpdateMesh();

    // --- ���� ������Ʈ ---
    UPrimitiveComponent* Owner = nullptr; // ���� ������Ʈ (��������)

    // --- LOD ---
    FVector CachedWorldPos; // Transform ���� �� ĳ�� ? LOD �Ÿ� ����
    uint32 CurrentLOD = 0;
    virtual void UpdateLOD(uint32 /*LODLevel*/) {}

    // --- Per-viewport ���� (bPerViewportUpdate=true ���Ͻø�) ---
    // �� ������, �� ����Ʈ�� ī�޶� �����ͷ� ���Ͻ� ���¸� ����
    virtual void UpdatePerViewport(const FSceneView& SceneView) {}

    // ���õ� ���Ͻ��� ���� ���� ������Ʈ���� ����� �ð�ȭ ����
    void CollectSelectedVisuals(FScene& Scene) const;

    // --- ���ü������� ---
    bool bVisible = true;
    bool bSelected = false;
    bool bSupportsOutline = true;
    bool bNeverCull = false; // true�� frustum culling ��󿡼� ���� (Gizmo �� ������ ���Ͻ�)
    bool bShowAABB = true;   // ���� �� AABB ����� ���� ǥ�� ���� (Billboard/SubUV ���� false)

    // --- ���� �н� ---
    ERenderPass Pass = ERenderPass::Opaque;

    // ��Ƽ���� ��� ���� ���� (���� ���ǿ� ������ ���� �� ���)
    EBlendState Blend = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState Rasterizer = ERasterizerState::SolidBackCull;

    // --- ĳ�̵� ���� ������ (��� �� �ʱ�ȭ, dirty �ø� ����) ---
    FShader* Shader = nullptr;
    FMeshBuffer* MeshBuffer = nullptr;
    FPerObjectConstants PerObjectConstants = {};
    FBoundingBox CachedBounds;
    mutable bool bPerObjectCBDirty = true;

    // ���Ǻ� ��ο� ���� (�޽�/��Ƽ���� ���� �ø� �籸��)
    TArray<FMeshSectionRenderData> SectionRenderData;

    // Ư�� CB (Gizmo, SubUV ��)
    FConstantBufferBinding ExtraCB;

    // �ؽ�ó/��Ƽ���� ���ε� (Billboard/SubUV/������ primitive ��������)
    ID3D11ShaderResourceView* DiffuseSRV = nullptr;
    ID3D11ShaderResourceView* NormalSRV = nullptr;
    ID3D11ShaderResourceView* SpecularSRV = nullptr;
    FConstantBuffer* MaterialCB[2] = {};

    // ����Ʈ�� ������ �ʿ��� ���Ͻ� (Gizmo, Billboard ��)
    bool bPerViewportUpdate = false;
    bool bFontBatched = false; // true�� FFontGeometry ��Ī ��� ��� (TextRenderProxy)
    bool bAllowViewModeShaderOverride = false; // true�� ViewMode Opaque/Decal/Lighting ���̴��� ��ü ����

    // ū �������� visible proxy ��� �� LOD ������ ������ �л��Ѵ�.
    uint32 LastLODUpdateFrame = UINT32_MAX;

    void MarkPerObjectCBDirty() const { bPerObjectCBDirty = true; }
    void ClearPerObjectCBDirty() const { bPerObjectCBDirty = false; }
    bool NeedsPerObjectCBUpload() const { return bPerObjectCBDirty; }
};
