#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "PhysicsEngine/BodySetup.h"
#include "Render/Geometry/CollisionDebugGeometry.h"

#include <algorithm>

namespace
{
	const FVector4 PhysicsBodyWireColor(0.2f, 1.0f, 0.45f, 1.0f);

	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
	{
		const uintptr_t AMat = reinterpret_cast<uintptr_t>(A.Material);
		const uintptr_t BMat = reinterpret_cast<uintptr_t>(B.Material);
		if (AMat != BMat)
			return AMat < BMat;

		return A.FirstIndex < B.FirstIndex;
	}

	void SortSectionDrawsByMaterial(TArray<FMeshSectionDraw>& Draws)
	{
		if (Draws.size() > 1)
		{
			std::sort(Draws.begin(), Draws.end(), SectionMaterialLess);
		}
	}
}

// ============================================================
// FStaticMeshSceneProxy
// ============================================================
FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::StaticMesh;
}

UStaticMeshComponent* FStaticMeshSceneProxy::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(GetOwner());
}

// ============================================================
// UpdateMaterial — 머티리얼만 변경된 경우 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FStaticMeshSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	FPrimitiveSceneProxy::AddReferencedObjects(Collector);
	for (const FLODDrawData& LOD : LODData)
	{
		for (const FMeshSectionDraw& Draw : LOD.SectionDraws)
		{
			Collector.AddReferencedObject(Draw.Material);
		}
	}
}

// ============================================================
// UpdateMesh — 메시 버퍼 + 셰이더 교체 후 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::BuildPhysicsBodyWireLines(const FFrameContext& /*Frame*/, TArray<FPhysicsDebugLine>& OutLines) const
{
	UStaticMeshComponent* SMC = GetStaticMeshComponent();
	UStaticMesh* Mesh = IsValid(SMC) ? SMC->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	Mesh->EnsureDefaultBodySetup();
	const UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (!BodySetup)
	{
		return;
	}

	FTransform ComponentWorldTM = FTransform::FromMatrixWithScale(SMC->GetWorldMatrix());
	const FVector ComponentScale = ComponentWorldTM.Scale;
	ComponentWorldTM.Scale = FVector::OneVector;
	FPhysicsBodyDebugGeometry::AddBodySetupWireLines(OutLines, BodySetup, ComponentWorldTM, ComponentScale, false, PhysicsBodyWireColor);
}

void FStaticMeshSceneProxy::UpdateMesh()
{
	if (!HasValidOwner())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		return;
	}

	UPrimitiveComponent* OwnerComp = GetOwner();
	MeshBuffer = IsValid(OwnerComp) ? OwnerComp->GetMeshBuffer() : nullptr;
	RebuildSectionDraws();
}

// ============================================================
// UpdateLOD — LOD 레벨 변경 시 MeshBuffer/SectionDraws 스왑
// ============================================================
void FStaticMeshSceneProxy::UpdateLOD(uint32 LODLevel)
{
	if (LODLevel >= LODCount) LODLevel = LODCount - 1;
	if (LODLevel == CurrentLOD) return;

	// 현재 활성 데이터를 LODData 슬롯에 swap (할당/해제 없는 O(1) 교환)
	std::swap(MeshBuffer, LODData[CurrentLOD].MeshBuffer);
	std::swap(SectionDraws, LODData[CurrentLOD].SectionDraws);

	// 새 LOD 데이터를 활성 슬롯에서 swap
	CurrentLOD = LODLevel;
	std::swap(MeshBuffer, LODData[LODLevel].MeshBuffer);
	std::swap(SectionDraws, LODData[LODLevel].SectionDraws);

}

// ============================================================
// RebuildSectionDraws — 모든 LOD의 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::RebuildSectionDraws()
{
	UStaticMeshComponent* SMC = GetStaticMeshComponent();
	if (!IsValid(SMC))
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		return;
	}
	UStaticMesh* Mesh = SMC->GetStaticMesh();
	if (!Mesh || !Mesh->GetStaticMeshAsset())
	{
		for (uint32 lod = 0; lod < MAX_LOD; ++lod)
		{
			LODData[lod].MeshBuffer = nullptr;
			LODData[lod].SectionDraws.clear();
		}

		LODCount = 1;
		CurrentLOD = 0;
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	const auto& Slots = Mesh->GetStaticMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();
	LODCount = Mesh->GetLODCount();

	// 각 LOD별 SectionDraws + MeshBuffer 구축
	for (uint32 lod = 0; lod < LODCount; ++lod)
	{
		const auto& Sections = Mesh->GetLODSections(lod);
		LODData[lod].MeshBuffer = Mesh->GetLODMeshBuffer(lod);
		LODData[lod].SectionDraws.clear();
		LODData[lod].SectionDraws.reserve(Sections.size());

		for (const FStaticMeshSection& Section : Sections)
		{
			FMeshSectionDraw Draw;
			Draw.FirstIndex = Section.FirstIndex;
			Draw.IndexCount = Section.NumTriangles * 3;

			int32 i = Section.MaterialIndex;
			if (i >= 0 && i < static_cast<int32>(Slots.size()))
			{
				if (i < static_cast<int32>(Overrides.size()) && Overrides[i])
					Draw.Material = Overrides[i];
				else if (Slots[i].MaterialInterface)
					Draw.Material = Slots[i].MaterialInterface;
			}

			LODData[lod].SectionDraws.push_back(Draw);
		}

		SortSectionDrawsByMaterial(LODData[lod].SectionDraws);
	}

	// LOD0을 활성 슬롯으로 설정
	CurrentLOD = 0;
	std::swap(MeshBuffer, LODData[0].MeshBuffer);
	std::swap(SectionDraws, LODData[0].SectionDraws);

}
