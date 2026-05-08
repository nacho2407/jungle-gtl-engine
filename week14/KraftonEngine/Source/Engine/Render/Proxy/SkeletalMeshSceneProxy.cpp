#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

USkeletalMeshComponent* FSkeletalMeshSceneProxy::GetSkeletalMeshComponent() const
{
	return static_cast<USkeletalMeshComponent*>(GetOwner());
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
};

void FSkeletalMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	RebuildSectionDraws();
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	const auto& Slots = Mesh->GetSkeletalMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();

	for (const FSkeletalMeshSection& Section : Mesh->GetSkeletalMeshAsset()->Sections)
	{
		FMeshSectionDraw Draw;
		Draw.Material = FMaterialManager::Get().GetOrCreateMaterial(Section.MaterialSlotName);
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.IndexCount;

		int32 i = Section.MaterialIndex;
		if (i >= 0 && i < static_cast<int32>(Slots.size()))
		{
			if (i < static_cast<int32>(Overrides.size()) && Overrides[i])
				Draw.Material = Overrides[i];
			else if (Slots[i].MaterialInterface)
				Draw.Material = Slots[i].MaterialInterface;
		}

		SectionDraws.push_back(Draw);
	}
}
