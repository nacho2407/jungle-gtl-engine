#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <cstring>

namespace
{
	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
	{
		const uintptr_t ASRV = reinterpret_cast<uintptr_t>(A.DiffuseSRV);
		const uintptr_t BSRV = reinterpret_cast<uintptr_t>(B.DiffuseSRV);
		if (ASRV != BSRV)
		{
			return ASRV < BSRV;
		}

		if (A.bIsUVScroll != B.bIsUVScroll)
		{
			return A.bIsUVScroll < B.bIsUVScroll;
		}

		const int ColorCmp = std::memcmp(&A.DiffuseColor, &B.DiffuseColor, sizeof(FVector4));
		if (ColorCmp != 0)
		{
			return ColorCmp < 0;
		}

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
}

UStaticMeshComponent* FStaticMeshSceneProxy::GetStaticMeshComponent() const
{
	return static_cast<UStaticMeshComponent*>(Owner);
}

// ============================================================
// UpdateMaterial — 머티리얼만 변경된 경우 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

// ============================================================
// UpdateMesh — 메시 버퍼 + 셰이더 교체 후 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::StaticMesh);
	Pass = ERenderPass::Opaque;

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
	UpdateSortKey();
}

// ============================================================
// RebuildSectionDraws — 모든 LOD의 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::RebuildSectionDraws()
{
	UStaticMeshComponent* SMC = GetStaticMeshComponent();
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
		UpdateSortKey();
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
				{
					UMaterial* Mat = Overrides[i];
					if (Mat->DiffuseTexture)
						Draw.DiffuseSRV = Mat->DiffuseTexture->GetSRV();
					Draw.DiffuseColor = Mat->DiffuseColor;
				}
			}

			LODData[lod].SectionDraws.push_back(Draw);
		}

		SortSectionDrawsByMaterial(LODData[lod].SectionDraws);
	}

	// LOD0을 활성 슬롯으로 설정
	CurrentLOD = 0;
	std::swap(MeshBuffer, LODData[0].MeshBuffer);
	std::swap(SectionDraws, LODData[0].SectionDraws);
	UpdateSortKey();
}
