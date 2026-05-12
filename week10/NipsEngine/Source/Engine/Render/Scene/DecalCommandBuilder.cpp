#include "DecalCommandBuilder.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Geometry/OBB.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"
#include "Runtime/Stats/ScopeCycleCounter.h"

#include <cmath>

namespace
{
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

void FDecalCommandBuilder::Reset()
{
    LastStats = {};
}

void FDecalCommandBuilder::CollectDecal(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                                        FMeshBufferManager& MeshBufferManager,
                                        FWorldSpatialIndex::FPrimitiveOBBQueryScratch& OBBQueryScratch)
{
    if (!ShowFlags.bDecals) return;

    FScopeCycleCounter RenderDecalScope({});

    UDecalComponent* DecalComp = static_cast<UDecalComponent*>(Primitive);
    UMaterialInterface* Material = Cast<UMaterialInterface>(DecalComp->GetMaterial());

    UWorld* World = DecalComp->GetOwner() ? DecalComp->GetOwner()->GetFocusedWorld() : nullptr;
    if (World == nullptr)
    {
        return;
    }

    FOBB DecalOBB = FOBB::FromAABB(DecalComp->GetWorldAABB(), DecalComp->GetWorldMatrix());

    TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
    World->GetSpatialIndex().OBBQueryPrimitives(DecalOBB, VisiblePrimitiveScratch, OBBQueryScratch);

    for (UPrimitiveComponent* Prim : VisiblePrimitiveScratch)
    {
        if (!Prim || !Prim->IsCastDecal()) continue;
        if (Prim->GetPrimitiveType() != EPrimitiveType::EPT_StaticMesh) continue;

        UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Prim);
        const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

        if (!StaticMesh || !StaticMesh->HasValidMeshData()) continue;

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
        if (!MeshBuffer) continue;

        const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
        const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];

            FRenderCommand Cmd = {};
            Cmd.Type = ERenderCommandType::Decal;
            Cmd.VertexFactoryType = EVertexFactoryType::Decal;
            Cmd.PerObjectConstants = FPerObjectConstants{ Prim->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;

            Cmd.Material = Material;

            Cmd.Constants.Decal.InvDecalWorld = DecalComp->GetDecalMatrix().GetInverse();
            Cmd.Constants.Decal.ColorTint = DecalComp->GetDecalColor().ToVector4();

            RenderBus.AddCommand(ERenderPass::Decal, Cmd);
        }
    }

    LastStats.TotalDecalCount += 1;
    LastStats.CollectTimeMS += static_cast<int32>(RenderDecalScope.Finish());
}
