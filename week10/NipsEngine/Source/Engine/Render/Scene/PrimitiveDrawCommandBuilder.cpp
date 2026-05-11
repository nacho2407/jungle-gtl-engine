#include "PrimitiveDrawCommandBuilder.h"

#include "Component/BillboardComponent.h"
#include "Component/FireballComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"

#include <cmath>

namespace
{
    FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
    {
        const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
        const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(Primitive);
        return UBillboardComponent::MakeBillboardWorldMatrix(
            WorldMatrix.GetOrigin(),
            Billboard->GetBillboardWorldScale(),
            RenderBus.GetCameraForward(),
            RenderBus.GetCameraRight(),
            RenderBus.GetCameraUp());
    }

    int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
    {
        bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
        if (ValidLODCount <= 1 || IsOrthoGraphic) return 0;

        const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
        const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
        const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

        const FVector Diff = Center - CameraPos;
        const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

        if (Dist <= 1e-4f) return 0;

        const float ProjectedRadius = (SphereRadius / Dist) * ProjMatrix.M[2][1];
        const float ScreenCoverage = ProjectedRadius;

        static constexpr float Thresholds[] = { 0.15f, 0.08f, 0.05f, 0.02f };
        static constexpr int32 ThresholdCount = static_cast<int32>(sizeof(Thresholds) / sizeof(Thresholds[0]));

        const int32 MaxLOD = ValidLODCount - 1;
        for (int32 LOD = 0; LOD < MaxLOD; ++LOD)
        {
            float Threshold = (LOD < ThresholdCount) ? Thresholds[LOD] : 0.0f;
            if (ScreenCoverage >= Threshold)
                return LOD;
        }

        return MaxLOD;
    }
}

bool FPrimitiveDrawCommandBuilder::CollectPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags,
                                                    EViewMode ViewMode, FRenderBus& RenderBus,
                                                    FMeshBufferManager& MeshBufferManager) const
{
    (void)ViewMode;

    if (Primitive == nullptr || !Primitive->IsVisible()) return true;

    switch (Primitive->GetPrimitiveType())
    {
    case EPrimitiveType::EPT_StaticMesh:
    {
        if (!ShowFlags.bPrimitives) return true;

        UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
        const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

        if (!StaticMesh || !StaticMesh->HasValidMeshData()) return true;

        FVector CameraPos = RenderBus.GetCameraPosition();
        FMatrix ProjMatrix = RenderBus.GetProj();
        FAABB Bounds = StaticMeshComp->GetWorldAABB();
        const int32 ValidLODCount = StaticMesh->GetValidLODCount();

        int32 SelectedLOD = 0;
        if (ShowFlags.bEnableLOD)
        {
            SelectedLOD = SelectLODLevel(CameraPos, Bounds, ProjMatrix, ValidLODCount);
        }

        FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, SelectedLOD);
        if (!MeshBuffer) return true;

        const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
        const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];
            UMaterialInterface* Material = Cast<UMaterialInterface>(StaticMeshComp->GetMaterial(SectionIdx));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;
            Cmd.Material = Material;

            Cmd.WorldAABB = StaticMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }

        return true;
    }

    case EPrimitiveType::EPT_SkeletalMesh:
    {
        if (!ShowFlags.bPrimitives) return true;

        USkeletalMeshComponent* SkeletalMeshComp = static_cast<USkeletalMeshComponent*>(Primitive);
        USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();

        if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData()) return true;

        SkeletalMeshComp->EnsureSkinningUpdated();
        const bool bNeedsUpload = SkeletalMeshComp->ConsumeRenderStateDirty();

        const TArray<FNormalVertex>& SkinnedVertices = SkeletalMeshComp->GetSkinnedVertices();
        const TArray<uint32>& Indices = SkeletalMesh->GetIndices(); // 이건 immutable이라 걍 asset에서 들고와도 댐

        FMeshBuffer* MeshBuffer = MeshBufferManager.GetSkeletalMeshBuffer(
            SkeletalMeshComp->GetUUID(),
            SkeletalMesh,
            SkinnedVertices,
            Indices,
            bNeedsUpload);
        if (!MeshBuffer) return true;

        const TArray<FStaticMeshSection>& Sections = SkeletalMesh->GetSections();
        if (Sections.empty()) // fallback
        {
            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.MeshBuffer = MeshBuffer;
            Cmd.SectionIndexStart = 0;
            Cmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
            Cmd.Material = SkeletalMeshComp->GetMaterial(0);
            Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
            return true;
        }

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];
            if (Section.IndexCount == 0)
            {
                continue;
            }

            UMaterialInterface* Material = Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(SectionIdx));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh; // 그리는 것 자체는 CPU Skinning에서는 StaticMesh 그대로 태워도 됨
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;
            Cmd.Material = Material;

            Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }

        return true;
    }

    case EPrimitiveType::EPT_Text:
    {
        if (!ShowFlags.bBillboardText) return true;

        UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
        const FFontResource* Font = TextComp->GetFont();
        if (!Font || !Font->IsLoaded()) return true;

        const FString& Text = TextComp->GetText();
        if (Text.empty()) return true;

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Font;
        Cmd.SourcePrimitive = Primitive;
        Cmd.PerObjectConstants = FPerObjectConstants{ TextComp->GetWorldMatrix(), TextComp->GetColor() };
        Cmd.Constants.Font.Text = &Text;
        Cmd.Constants.Font.Font = Font;
        Cmd.Constants.Font.Scale = TextComp->GetFontSize();

        RenderBus.AddCommand(ERenderPass::Font, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_SubUV:
    {
        USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
        const FParticleResource* Particle = SubUVComp->GetParticle();
        if (!Particle || !Particle->IsLoaded()) return true;

        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{
            MakeViewBillboardMatrix(Primitive, RenderBus),
            FColor::White().ToVector4() };
        Cmd.SourcePrimitive = Primitive;
        Cmd.Type = ERenderCommandType::SubUV;
        Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_SubUV);
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
        Cmd.Constants.SubUV.Particle = Particle;
        Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
        Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
        Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_Billboard:
    {
        UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Primitive);
        UTexture* Texture = BillboardComp->GetTexture();

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Billboard;
        Cmd.SourcePrimitive = Primitive;
        Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Billboard);
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
        Cmd.PerObjectConstants = FPerObjectConstants{
            MakeViewBillboardMatrix(Primitive, RenderBus),
            FColor::White().ToVector4() };
        Cmd.Constants.Billboard.Texture = Texture;
        Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
        Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
        Cmd.Constants.Billboard.Color = BillboardComp->GetColor();

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_FOG:
    {
        if (!ShowFlags.bFog)
            return true;
        UHeightFogComponent* HeightFogComp = static_cast<UHeightFogComponent*>(Primitive);

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Primitive;
        Cmd.Constants.Fog.FogDensity = HeightFogComp->GetFogDensity();
        Cmd.Constants.Fog.FogColor = HeightFogComp->GetFogInscatteringColor();
        Cmd.Constants.Fog.HeightFalloff = HeightFogComp->GetHeightFalloff();
        Cmd.Constants.Fog.FogHeight = HeightFogComp->GetFogHeight();
        Cmd.Constants.Fog.FogStartDistance = HeightFogComp->GetFogStartDistance();
        Cmd.Constants.Fog.FogMaxOpacity = HeightFogComp->GetFogMaxOpacity();
        Cmd.Constants.Fog.FogCutoffDistance = HeightFogComp->GetFogCutoffDistance();

        RenderBus.AddCommand(ERenderPass::Fog, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_Fireball:
    {
        UFireballComponent* FireballComp = static_cast<UFireballComponent*>(Primitive);

        FLightData LightData = {};
        LightData.Intensity = FireballComp->GetIntensity();
        LightData.Radius = FireballComp->GetRadius();
        LightData.RadiusFalloff = FireballComp->GetRadiusFallOff();
        LightData.WorldPos = FireballComp->GetWorldLocation();

        FColor Color = FireballComp->GetLinearColor();
        LightData.Color.X = Color.R;
        LightData.Color.Y = Color.G;
        LightData.Color.Z = Color.B;
        return true;
    }

    case EPrimitiveType::EPT_ProceduralMesh:
    {
        if (!ShowFlags.bPrimitives)
            return true;

        UProceduralMeshComponent* ProcMeshComp = static_cast<UProceduralMeshComponent*>(Primitive);
        const TArray<UProceduralMeshComponent::FMeshSection>& Sections = ProcMeshComp->GetSections();

        if (!ProcMeshComp || Sections.empty())
            return true;

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const UProceduralMeshComponent::FMeshSection& Section = Sections[SectionIdx];
            FMeshBuffer* MeshBuffer = nullptr;
            MeshBuffer = MeshBufferManager.GetProcMeshBuffer(ProcMeshComp->GetUUID(), Section.Vertices, Section.Indices);

            if (!MeshBuffer)
                break;

            UMaterialInterface* Material = Cast<UMaterialInterface>(ProcMeshComp->GetMaterial(SectionIdx));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = 0;
            Cmd.SectionIndexCount = static_cast<uint32>(Section.Indices.size());
            Cmd.Material = Material;

            Cmd.WorldAABB = ProcMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }
        return true;
    }

    default:
        return false;
    }
}
