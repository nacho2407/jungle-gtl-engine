#include "EditorOverlayCollector.h"

#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Core/ResourceManager.h"
#include "Engine/Asset/SkeletalMesh.h"
#include "Engine/Asset/StaticMesh.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"
#include "Spatial/WorldSpatialIndex.h"

namespace
{
    FColor MakeBVHInternalNodeColor(int32 PathIndexFromLeaf, int32 PathLength)
    {
        if (PathLength <= 1)
        {
            return FColor::Yellow();
        }

        const float T = static_cast<float>(PathIndexFromLeaf) / static_cast<float>(PathLength - 1);
        return FColor::Lerp(FColor::Cyan(), FColor::Yellow(), T);
    }

    FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
    {
        const FVector WorldScale = SubUVComp->GetBillboardWorldScale();
        return UBillboardComponent::MakeBillboardWorldMatrix(
            SubUVComp->GetWorldLocation(),
            FVector(
                WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
                SubUVComp->GetWidth() * WorldScale.Y * 0.5f,
                SubUVComp->GetHeight() * WorldScale.Z * 0.5f),
            RenderBus.GetCameraForward(),
            RenderBus.GetCameraRight(),
            RenderBus.GetCameraUp());
    }

    FMatrix MakeViewBillboardMaskMatrix(const UBillboardComponent* Billboard, const FRenderBus& RenderBus)
    {
        const FVector WorldScale = Billboard->GetBillboardWorldScale();
        return UBillboardComponent::MakeBillboardWorldMatrix(
            Billboard->GetWorldLocation(),
            FVector(
                WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
                Billboard->GetWidth() * WorldScale.Y * 0.5f,
                Billboard->GetHeight() * WorldScale.Z * 0.5f),
            RenderBus.GetCameraForward(),
            RenderBus.GetCameraRight(),
            RenderBus.GetCameraUp());
    }

    FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
    {
        static constexpr FVector LocalQuadCorners[4] =
        {
            FVector(0.0f, -0.5f,  0.5f),
            FVector(0.0f,  0.5f,  0.5f),
            FVector(0.0f,  0.5f, -0.5f),
            FVector(0.0f, -0.5f, -0.5f)
        };

        FAABB Box;
        Box.Reset();

        for (const FVector& Corner : LocalQuadCorners)
        {
            Box.Expand(WorldMatrix.TransformPosition(Corner));
        }

        return Box;
    }

    FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
    {
        switch (PrimitiveComponent->GetPrimitiveType())
        {
        case EPrimitiveType::EPT_Billboard:
        {
            const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(PrimitiveComponent);
            return BuildQuadAABB(MakeViewBillboardMaskMatrix(Billboard, RenderBus));
        }
        case EPrimitiveType::EPT_Text:
        {
            const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
            return BuildQuadAABB(TextComp->GetTextMatrix());
        }
        case EPrimitiveType::EPT_SubUV:
        {
            const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
            return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
        }

        default:
            return PrimitiveComponent->GetWorldAABB();
        }
    }
}

void FEditorOverlayCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags,
                                               EViewMode ViewMode, FRenderBus& RenderBus,
                                               FMeshBufferManager& MeshBufferManager,
                                               bool bIncludeEditorOnlyPrimitives) const
{
    bool bHasSelectionMask = false;
    for (AActor* Actor : SelectedActors)
    {
        bHasSelectionMask |= CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus, MeshBufferManager, bIncludeEditorOnlyPrimitives);
    }

    if (bHasSelectionMask)
    {
        FRenderCommand PostProcessCmd = {};
        PostProcessCmd.Type = ERenderCommandType::PostProcessOutline;
        PostProcessCmd.Material = FResourceManager::Get().GetMaterial("OutlineMaterial");

        UMaterial* Material = Cast<UMaterial>(PostProcessCmd.Material);
        Material->SetVector2("OutlineViewportSize", RenderBus.GetViewportSize());
        Material->SetVector2("OutlineViewportOrigin", RenderBus.GetViewportOrigin());
        Material->DepthStencilType = EDepthStencilType::DepthReadOnly;
        Material->RasterizerType = ERasterizerType::SolidBackCull;
        Material->BlendType = EBlendType::AlphaBlend;

        RenderBus.AddCommand(ERenderPass::PostProcessOutline, PostProcessCmd);
    }
}

void FEditorOverlayCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic) const
{
    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::Grid;
    Cmd.VertexFactoryType = EVertexFactoryType::Line;
    Cmd.Constants.Grid.GridSpacing = GridSpacing;
    Cmd.Constants.Grid.GridHalfLineCount = GridHalfLineCount;
    Cmd.Constants.Grid.bOrthographic = bOrthographic;
    RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FEditorOverlayCollector::CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus) const
{
    if (!SkComp || !SkComp->HasValidMesh()) return;

    const USkeletalMesh* Mesh = SkComp->GetSkeletalMesh();
    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    const FVector4 BoneColor(1.0f, 0.85f, 0.0f, 1.0f);   // 노란빛 — 본 와이어 기본 색

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 ParentIdx = Bones[i].ParentIndex;
        if (ParentIdx < 0) continue;   // 루트는 부모가 없어 선 1개를 그릴 수 없음

        const FMatrix ChildWorld  = SkComp->GetBoneWorldMatrix(i);
        const FMatrix ParentWorld = SkComp->GetBoneWorldMatrix(ParentIdx);

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::DebugLine;
        Cmd.Constants.Line.Start = ParentWorld.GetTranslation();
        Cmd.Constants.Line.End   = ChildWorld.GetTranslation();
        Cmd.Constants.Line.Color = BoneColor;
        RenderBus.AddCommand(ERenderPass::Editor, Cmd);
    }
}

void FEditorOverlayCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                                           FMeshBufferManager& MeshBufferManager, bool bIsActiveOperation) const
{
    if (ShowFlags.bGizmo == false) return;
    if (!Gizmo || !Gizmo->IsVisible()) return;

    FMeshBuffer* GizmoMesh = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
    FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
    bool bHolding = Gizmo->IsHolding();
    int32 SelectedAxis = Gizmo->GetSelectedAxis();

    auto CreateGizmoCmd = [&](bool bInner) {
        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Gizmo;
        Cmd.VertexFactoryType = EVertexFactoryType::Gizmo;
        Cmd.MeshBuffer = GizmoMesh;

        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = GizmoMesh->GetIndexBuffer().GetIndexCount();

        Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

        UMaterial* Material = Cast<UMaterial>(Gizmo->GetMaterial());
        Cmd.Material = Material;

        if (bInner)
        {
            Material->DepthStencilType = EDepthStencilType::GizmoInside;
            Material->BlendType = EBlendType::AlphaBlend;
        }
        else
        {
            Material->DepthStencilType = EDepthStencilType::GizmoOutside;
            Material->BlendType = EBlendType::Opaque;
        }

        Material->SetVector4("GizmoColorTint", FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        Material->SetBool("bIsInnerGizmo", bInner);
        Material->SetBool("bClicking", bHolding);
        Material->SetUInt("SelectedAxis", (SelectedAxis >= 0 && bIsActiveOperation) ? (uint32)SelectedAxis : 0xffffffffu);
        Material->SetFloat("HoveredAxisOpacity", 0.3f);

        return Cmd;
        };

    RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

    if (!bHolding)
    {
        RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
    }
}

bool FEditorOverlayCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                                       FRenderBus& RenderBus, FMeshBufferManager& MeshBufferManager,
                                                       bool bIncludeEditorOnlyPrimitives) const
{
    (void)ViewMode;

    if (!Actor->IsVisible()) return false;

    bool bHasSelectionMask = false;
    std::unordered_set<int32> SeenBVHNodeIndices;

    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(Comp))
        {
            CollectDirectionalLightCommand(DirLight, ShowFlags, RenderBus);
        }
        else if (USpotlightComponent* Spotlight = Cast<USpotlightComponent>(Comp))
        {
            CollectSpotLightCommand(Spotlight, ShowFlags, RenderBus);
        }
        else if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(Comp))
        {
            CollectPointLightCommand(PointLight, ShowFlags, RenderBus);
        }
    }

    for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
    {
        if (!primitiveComponent->IsVisible()) continue;
        if (!bIncludeEditorOnlyPrimitives && primitiveComponent->IsEditorOnly())
        {
            UWorld* World = Actor->GetFocusedWorld();
            if (World && World->GetWorldType() != EWorldType::Editor)
                continue;
        }

        FMeshBuffer* MeshBuffer = nullptr;
        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
        {
            auto* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
            MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh());
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SkeletalMesh)
        {
            auto* SkeletalMeshComp = static_cast<USkeletalMeshComponent*>(primitiveComponent);
            USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();
            if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData()) continue;

            // 메인 render pass(CollectWorld)가 이 함수 *전*에 같은 프레임에 돌면서
            // skinning + 버퍼 업로드를 이미 끝낸 상태. 여기서는 dirty flag를 소비하지 않고
            // bNeedsUpload=false로 캐시된 버퍼만 가져온다.
            SkeletalMeshComp->EnsureSkinningUpdated();
            MeshBuffer = MeshBufferManager.GetSkeletalMeshBuffer(
                SkeletalMeshComp->GetUUID(),
                SkeletalMesh,
                SkeletalMeshComp->GetSkinnedVertices(),
                SkeletalMesh->GetIndices(),
                /*bNeedsUpload=*/ false);
        }
        else
        {
            MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
        }

        if (!MeshBuffer)
        {
            continue;
        }

        FRenderCommand BaseCmd{};
        BaseCmd.MeshBuffer = MeshBuffer;
        BaseCmd.SourcePrimitive = primitiveComponent;
        BaseCmd.PerObjectConstants = FPerObjectConstants(primitiveComponent->GetWorldMatrix());
        BaseCmd.SectionIndexStart = 0;
        BaseCmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
        BaseCmd.VertexFactoryType = EVertexFactoryType::Primitive;

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
        {
            BaseCmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SkeletalMesh)
        {
            BaseCmd.VertexFactoryType = EVertexFactoryType::SkeletalMesh;
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
        {
            BaseCmd.VertexFactoryType = EVertexFactoryType::Text;
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
        {
            BaseCmd.VertexFactoryType = EVertexFactoryType::SubUV;
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
        {
            BaseCmd.VertexFactoryType = EVertexFactoryType::Billboard;
        }

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
        {
            UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
            const FFontResource* Font = TextComp->GetFont();
            if (!Font || !Font->IsLoaded()) continue;
            const FString& Text = TextComp->GetText();
            if (Text.empty()) continue;

            FMatrix WorldMatrix = TextComp->GetTextMatrix();

            FRenderCommand TextCmd = BaseCmd;
            BaseCmd.PerObjectConstants = FPerObjectConstants(WorldMatrix);
            TextCmd.PerObjectConstants = FPerObjectConstants(TextComp->GetWorldMatrix(), TextComp->GetColor());
            TextCmd.Type = ERenderCommandType::Font;
            TextCmd.Constants.Font.Text = &Text;
            TextCmd.Constants.Font.Font = Font;
            TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
            RenderBus.AddCommand(ERenderPass::Font, TextCmd);
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
        {
            USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(primitiveComponent);
            const FParticleResource* Particle = SubUVComp->GetParticle();
            if (!Particle || !Particle->IsLoaded()) continue;

            BaseCmd.PerObjectConstants.Model = MakeViewSubUVSelectionMatrix(
                SubUVComp,
                RenderBus);
            BaseCmd.Constants.SubUV.Particle = Particle;
            BaseCmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
            BaseCmd.Constants.SubUV.Width = SubUVComp->GetWidth();
            BaseCmd.Constants.SubUV.Height = SubUVComp->GetHeight();
        }

        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
        {
            UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(primitiveComponent);
            BaseCmd.PerObjectConstants.Model = MakeViewBillboardMaskMatrix(BillboardComp, RenderBus);
            BaseCmd.Constants.Billboard.Texture = BillboardComp->GetTexture();
            BaseCmd.Constants.Billboard.Width = BillboardComp->GetWidth();
            BaseCmd.Constants.Billboard.Height = BillboardComp->GetHeight();
            BaseCmd.Constants.Billboard.Color = BillboardComp->GetColor();
        }

        if (!primitiveComponent->SupportsOutline()) continue;

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
        {
            UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
            const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
            const FStaticMesh* MeshData = StaticMesh ? StaticMesh->GetMeshData() : nullptr;
            const TArray<FStaticMeshSection>& Sections = MeshData ? MeshData->Sections : TArray<FStaticMeshSection>();

            if (!Sections.empty())
            {
                for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
                {
                    const FStaticMeshSection& Section = Sections[SectionIdx];
                    if (Section.IndexCount == 0)
                    {
                        continue;
                    }

                    FRenderCommand MaskCmd = BaseCmd;
                    MaskCmd.Type = ERenderCommandType::SelectionMask;
                    MaskCmd.SectionIndexStart = Section.StartIndex;
                    MaskCmd.SectionIndexCount = Section.IndexCount;
                    MaskCmd.Material = Cast<UMaterialInterface>(StaticMeshComp->GetMaterial(SectionIdx));
                    RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
                }
            }
            else
            {
                FRenderCommand MaskCmd = BaseCmd;
                MaskCmd.Type = ERenderCommandType::SelectionMask;
                MaskCmd.Material = Cast<UMaterialInterface>(StaticMeshComp->GetMaterial(0));
                RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
            }
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SkeletalMesh)
        {
            USkeletalMeshComponent* SkeletalMeshComp = static_cast<USkeletalMeshComponent*>(primitiveComponent);
            const USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();
            const TArray<FStaticMeshSection>& Sections = SkeletalMesh ? SkeletalMesh->GetSections() : TArray<FStaticMeshSection>();

            if (!Sections.empty())
            {
                for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
                {
                    const FStaticMeshSection& Section = Sections[SectionIdx];
                    if (Section.IndexCount == 0)
                    {
                        continue;
                    }

                    FRenderCommand MaskCmd = BaseCmd;
                    MaskCmd.Type = ERenderCommandType::SelectionMask;
                    MaskCmd.SectionIndexStart = Section.StartIndex;
                    MaskCmd.SectionIndexCount = Section.IndexCount;
                    MaskCmd.Material = Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(SectionIdx));
                    RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
                }
            }
            else
            {
                FRenderCommand MaskCmd = BaseCmd;
                MaskCmd.Type = ERenderCommandType::SelectionMask;
                MaskCmd.Material = Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(0));
                RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
            }
        }
        else
        {
            FRenderCommand MaskCmd = BaseCmd;
            MaskCmd.Type = ERenderCommandType::SelectionMask;
            RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
        }
        bHasSelectionMask = true;

        UDecalComponent* DecalComp = Cast<UDecalComponent>(primitiveComponent);
        if (DecalComp)
        {
            CollectOBBCommand(primitiveComponent, ShowFlags, RenderBus);
        }
        else
        {
            CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
        }

        CollectBVHInternalNodeAABBs(primitiveComponent, ShowFlags, RenderBus, SeenBVHNodeIndices);
    }

    return bHasSelectionMask;
}

void FEditorOverlayCollector::CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                                          FRenderBus& RenderBus, std::unordered_set<int32>& SeenNodeIndices) const
{
    if (!ShowFlags.bBoundingVolume || !ShowFlags.bBVHBoundingVolume || PrimitiveComponent == nullptr)
    {
        return;
    }

    AActor* Owner = PrimitiveComponent->GetOwner();
    UWorld* World = Owner ? Owner->GetFocusedWorld() : nullptr;
    if (World == nullptr)
    {
        return;
    }

    const FWorldSpatialIndex& SpatialIndex = World->GetSpatialIndex();
    const int32 ObjectIndex = SpatialIndex.FindObjectIndex(PrimitiveComponent);
    if (ObjectIndex == FBVH::INDEX_NONE)
    {
        return;
    }

    const FBVH& BVH = SpatialIndex.GetBVH();
    const TArray<int32>& ObjectToLeafNode = BVH.GetObjectToLeafNode();
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return;
    }

    const int32 LeafNodeIndex = ObjectToLeafNode[ObjectIndex];
    if (LeafNodeIndex == FBVH::INDEX_NONE)
    {
        return;
    }

    const TArray<FBVH::FNode>& Nodes = BVH.GetNodes();
    if (LeafNodeIndex < 0 || LeafNodeIndex >= static_cast<int32>(Nodes.size()))
    {
        return;
    }

    TArray<int32> PathToRoot;
    PathToRoot.reserve(16);

    int32 CurrentNodeIndex = Nodes[LeafNodeIndex].Parent;
    while (CurrentNodeIndex != FBVH::INDEX_NONE)
    {
        if (CurrentNodeIndex < 0 || CurrentNodeIndex >= static_cast<int32>(Nodes.size()))
        {
            break;
        }

        PathToRoot.push_back(CurrentNodeIndex);
        CurrentNodeIndex = Nodes[CurrentNodeIndex].Parent;
    }

    for (int32 PathIndex = 0; PathIndex < static_cast<int32>(PathToRoot.size()); ++PathIndex)
    {
        const int32 NodeIndex = PathToRoot[PathIndex];
        if (!SeenNodeIndices.insert(NodeIndex).second)
        {
            continue;
        }

        const FBVH::FNode& Node = Nodes[NodeIndex];
        if (Node.IsLeaf())
        {
            continue;
        }

        const FColor Color = MakeBVHInternalNodeColor(PathIndex, static_cast<int32>(PathToRoot.size()));
        CollectAABBCommand(Node.Bounds, Color, RenderBus);
    }
}

void FEditorOverlayCollector::CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus) const
{
    FRenderCommand AABBCmd = {};
    AABBCmd.Type = ERenderCommandType::DebugBox;
    AABBCmd.Constants.AABB.Min = Box.Min;
    AABBCmd.Constants.AABB.Max = Box.Max;
    AABBCmd.Constants.AABB.Color = Color;
    RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}

void FEditorOverlayCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const
{
    if (!ShowFlags.bBoundingVolume) return;

    const FAABB Box = BuildRenderAABB(PrimitiveComponent, RenderBus);
    CollectAABBCommand(Box, FColor::White(), RenderBus);
}

void FEditorOverlayCollector::CollectOBBCommand(const FOBB& Box, const FColor& Color, FRenderBus& RenderBus) const
{
    FRenderCommand OBBCmd = {};
    OBBCmd.Type = ERenderCommandType::DebugOBB;
    OBBCmd.Constants.OBB.Center = Box.Center;
    OBBCmd.Constants.OBB.Extents = Box.Extents;
    OBBCmd.Constants.OBB.Rotation = Box.Rotation.ToMatrix();
    OBBCmd.Constants.OBB.Color = Color;
    RenderBus.AddCommand(ERenderPass::Editor, OBBCmd);
}

void FEditorOverlayCollector::CollectOBBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const
{
    if (!ShowFlags.bBoundingVolume) return;

    const FAABB AABB = PrimitiveComponent->GetWorldAABB();
    const FOBB Box = FOBB::FromAABB(AABB, PrimitiveComponent->GetWorldMatrix());
    CollectOBBCommand(Box, FColor::Green(), RenderBus);
}

void FEditorOverlayCollector::CollectDirectionalLightCommand(const UDirectionalLightComponent* DirLight, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const
{
    if (!ShowFlags.bBoundingVolume) return;

    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::DebugDirectionalLight;
    Cmd.Constants.DirectionalLight.Position = DirLight->GetWorldLocation();
    Cmd.Constants.DirectionalLight.Direction = DirLight->GetForwardVector();
    Cmd.Constants.DirectionalLight.Color = FColor::Yellow();
    RenderBus.AddCommand(ERenderPass::Editor, Cmd);
}

void FEditorOverlayCollector::CollectPointLightCommand(const UPointLightComponent* PointLight, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const
{
    if (!ShowFlags.bBoundingVolume) return;
    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::DebugPointLight;
    Cmd.Constants.PointLight.Position = PointLight->GetWorldLocation();
    Cmd.Constants.PointLight.Range = PointLight->AttenuationRadius;
    Cmd.Constants.PointLight.Color = FColor::Yellow();
    RenderBus.AddCommand(ERenderPass::Editor, Cmd);
}

void FEditorOverlayCollector::CollectSpotLightCommand(const USpotlightComponent* Spotlight, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const
{
    if (!ShowFlags.bBoundingVolume) return;

    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::DebugSpotlight;
    Cmd.Constants.SpotLight.Position = Spotlight->GetWorldLocation();
    Cmd.Constants.SpotLight.Direction = Spotlight->GetForwardVector();
    Cmd.Constants.SpotLight.InnerAngle = Spotlight->InnerConeAngle;
    Cmd.Constants.SpotLight.OuterAngle = Spotlight->OuterConeAngle;
    Cmd.Constants.SpotLight.Range = Spotlight->AttenuationRadius;
    Cmd.Constants.SpotLight.Color = FColor::Yellow();
    RenderBus.AddCommand(ERenderPass::Editor, Cmd);
}
