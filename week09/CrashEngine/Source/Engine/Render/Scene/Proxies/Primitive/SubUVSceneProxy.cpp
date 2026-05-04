// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/SubUVSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>

namespace
{
void EnsureSubUVRegionCB(FConstantBuffer& Buffer)
{
    if (Buffer.GetBuffer() || !GEngine)
    {
        return;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    if (Device)
    {
        Buffer.Create(Device, sizeof(FSubUVRegionCBData));
    }
}
} // namespace

// ============================================================
// FSubUVSceneProxy
// ============================================================
FSubUVSceneProxy::FSubUVSceneProxy(USubUVComponent* InComponent)
    : FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
    bShowAABB = false;
    EnsureSubUVRegionCB(UVRegionCB);
}

FSubUVSceneProxy::~FSubUVSceneProxy()
{
    UVRegionCB.Release();
}

void FSubUVSceneProxy::UpdateMesh()
{
    FBillboardSceneProxy::UpdateMesh();

    USubUVComponent* Comp = GetSubUVComponent();
    EnsureSubUVRegionCB(UVRegionCB);

    // TexturedQuad (FVertexPNCT with UVs) for rendering
    MeshBuffer   = &FMeshBufferManager::Get().GetMeshBuffer(EPrimitiveMeshShape::TexturedQuad);
    Shader       = FShaderManager::Get().GetShader(EShaderType::SubUV);
    Pass         = ERenderPass::AlphaBlend;
    Blend        = EBlendState::AlphaBlend;
    DepthStencil = EDepthStencilState::DepthReadOnly;
    Rasterizer   = ERasterizerState::SolidNoCull;

    MaterialCB[0] = &UVRegionCB;

    if (UMaterial* Material = Comp->GetSubUVMaterial())
    {
        const FMaterialRenderState& RenderState = Material->GetRenderState();
        Pass                                   = RenderState.RenderPass;
        Blend                                  = RenderState.Blend;
        DepthStencil                           = RenderState.DepthStencil;
        Rasterizer                             = RenderState.Rasterizer;

        if (UTexture2D* Texture = Material->GetDiffuseTexture())
        {
            DiffuseSRV = Texture->GetSRV();
        }
    }

    if (!DiffuseSRV)
    {
        const FParticleResource* Particle = Comp->GetParticle();
        if (Particle && Particle->IsLoaded())
        {
            DiffuseSRV = Particle->SRV;
        }
    }
}

void FSubUVSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    USubUVComponent* Comp = GetSubUVComponent();
    bVisible              = Comp->ShouldRenderInCurrentWorld();
    if (!bVisible)
        return;

    ID3D11ShaderResourceView* CurrentSRV = nullptr;
    if (UMaterial* Material = Comp->GetSubUVMaterial())
    {
        const FMaterialRenderState& RenderState = Material->GetRenderState();
        Pass                                   = RenderState.RenderPass;
        Blend                                  = RenderState.Blend;
        DepthStencil                           = RenderState.DepthStencil;
        Rasterizer                             = RenderState.Rasterizer;

        if (UTexture2D* Texture = Material->GetDiffuseTexture())
        {
            CurrentSRV = Texture->GetSRV();
        }
    }

    const FParticleResource* Particle = Comp->GetParticle();
    if (!CurrentSRV && Particle && Particle->IsLoaded())
    {
        CurrentSRV = Particle->SRV;
    }

    if (!CurrentSRV)
    {
        bVisible = false;
        return;
    }

    DiffuseSRV = CurrentSRV;

    // Keep the same local-axis convention as UBillboardComponent:
    // local X = depth/normal, local Y = width, local Z = height.
    FMatrix BaseWorldMatrix;
    if (Comp->GetParent())
    {
        BaseWorldMatrix = Comp->GetRelativeMatrix() * Comp->GetParent()->GetWorldMatrix();
    }
    else
    {
        BaseWorldMatrix = Comp->GetRelativeMatrix();
    }

    // 스케일과 위치를 오염되지 않은 BaseWorldMatrix에서 추출합니다.
    FVector TrueWorldScale(
        FVector(BaseWorldMatrix.M[0][0], BaseWorldMatrix.M[0][1], BaseWorldMatrix.M[0][2]).Length(),
        FVector(BaseWorldMatrix.M[1][0], BaseWorldMatrix.M[1][1], BaseWorldMatrix.M[1][2]).Length(),
        FVector(BaseWorldMatrix.M[2][0], BaseWorldMatrix.M[2][1], BaseWorldMatrix.M[2][2]).Length());

    // 극단적인 예외 상황을 위한 안전장치
    if (std::isnan(TrueWorldScale.X) || std::isnan(TrueWorldScale.Y) || std::isnan(TrueWorldScale.Z))
    {
        TrueWorldScale = FVector(1.0f, 1.0f, 1.0f);
    }

    FVector SafeWorldLocation(BaseWorldMatrix.M[3][0], BaseWorldMatrix.M[3][1], BaseWorldMatrix.M[3][2]);

    const FVector SpriteScale(
        std::max(TrueWorldScale.X, 1.0f),
        Comp->GetWidth() * TrueWorldScale.Y,
        Comp->GetHeight() * TrueWorldScale.Z);

    FMatrix BillboardMatrix;
    if (Comp->IsBillboardEnabled())
    {
        FVector Forward = SceneView.CameraForward * -1.0f;
        if (Forward.Dot(Forward) < 0.0001f)
        {
            Forward = FVector(0.0f, 0.0f, -1.0f);
        }
        else
        {
            Forward.Normalize();
        }

        FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);
        if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
        {
            WorldUp = FVector(0.0f, 1.0f, 0.0f);
        }

        FVector Right = WorldUp.Cross(Forward).Normalized();
        FVector Up    = Forward.Cross(Right).Normalized();

        FMatrix RotMatrix;
        RotMatrix.SetAxes(Forward, Right, Up);

        // 오염된 GetWorldLocation() 대신 SafeWorldLocation 사용
        BillboardMatrix = FMatrix::MakeScaleMatrix(SpriteScale) * RotMatrix * FMatrix::MakeTranslationMatrix(SafeWorldLocation);
    }
    else
    {
        const FVector WorldScale = Comp->GetWorldScale();
        const FVector SpriteScale(
            std::max(WorldScale.X, 1.0f),
            Comp->GetWidth() * WorldScale.Y,
            Comp->GetHeight() * WorldScale.Z);
        BillboardMatrix = FMatrix::MakeScaleMatrix(SpriteScale) * Comp->GetRelativeMatrix() * FMatrix::MakeTranslationMatrix(Comp->GetWorldLocation());
    }

    PerObjectConstants = FPerObjectCBData::FromWorldMatrix(BillboardMatrix);
    MarkPerObjectCBDirty();

    const uint32 Cols = Particle ? Particle->Columns : static_cast<uint32>(std::max(Comp->GetCellCountX(), 1));
    const uint32 Rows = Particle ? Particle->Rows : static_cast<uint32>(std::max(Comp->GetCellCountY(), 1));
    if (Cols > 0 && Rows > 0)
    {
        const float  FrameW   = 1.0f / static_cast<float>(Cols);
        const float  FrameH   = 1.0f / static_cast<float>(Rows);
        const uint32 FrameIdx = Comp->GetFrameIndex();
        const uint32 Col      = FrameIdx % Cols;
        const uint32 Row      = FrameIdx / Cols;
        const float  Left     = std::max(0.0f, Comp->GetLeftOffset());
        const float  Right    = std::max(0.0f, Comp->GetRightOffset());
        const float  Top      = std::max(0.0f, Comp->GetTopOffset());
        const float  Bottom   = std::max(0.0f, Comp->GetBottomOffset());

        FSubUVRegionCBData& Region = ExtraCB.As<FSubUVRegionCBData>();
        Region.U                   = Col * FrameW + Left;
        Region.V                   = Row * FrameH + Top;
        Region.Width               = std::max(0.0f, FrameW - Left - Right);
        Region.Height              = std::max(0.0f, FrameH - Top - Bottom);

		ID3D11DeviceContext* Context = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
        if (Context && UVRegionCB.GetBuffer())
        {
            UVRegionCB.Update(Context, &Region, sizeof(FSubUVRegionCBData));
        }
        
    }
}

USubUVComponent* FSubUVSceneProxy::GetSubUVComponent() const
{
    return static_cast<USubUVComponent*>(Owner);
}
