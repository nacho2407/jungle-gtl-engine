#include "SkinnedMeshComponent.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

DEFINE_CLASS(USkinnedMeshComponent, UMeshComponent)

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    if (SkeletalMesh == InSkeletalMesh)
    {
        return;
    }

    SkeletalMesh = InSkeletalMesh;
    Materials.clear();
    CurrentLocalPose.clear();
    CurrentGlobalPose.clear();
    SkinningMatrices.clear();
    SkinnedVertices.clear();

    if (SkeletalMesh)
    {
        SkeletalMeshPath = SkeletalMesh->GetAssetPathFileName();

        const TArray<FStaticMeshSection>& Sections = SkeletalMesh->GetSections();
        const TArray<FStaticMeshMaterialSlot>& Slots = SkeletalMesh->GetMaterialSlots();

        Materials.reserve(Sections.size());
        for (int32 SectionIndex = 0; SectionIndex < static_cast<int32>(Sections.size()); ++SectionIndex)
        {
            const int32 SlotIndex = Sections[SectionIndex].MaterialSlotIndex;
            if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(Slots.size()))
            {
                Materials.push_back(Slots[SlotIndex].Material);
            }
            else
            {
                Materials.push_back(nullptr);
            }
        }

        InitializePoseFromBindPose();
        SkinnedVertices.resize(SkeletalMesh->GetVertices().size());
    }
    else
    {
        SkeletalMeshPath.clear();
    }

    MarkBoundsDirty();
    MarkRenderStateDirty();
    MarkSkinningDirty();
}

// 작성중