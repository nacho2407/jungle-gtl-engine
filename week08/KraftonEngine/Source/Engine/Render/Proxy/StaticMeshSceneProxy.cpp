#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

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

	// 메시 변경 시 머티리얼도 같이 재구축
	RebuildSectionDraws();
}

// ============================================================
// RebuildSectionDraws — Owner의 머티리얼/섹션 데이터로 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::RebuildSectionDraws()
{
	UStaticMeshComponent* SMC = GetStaticMeshComponent();
	UStaticMesh* Mesh = SMC->GetStaticMesh();
	if (!Mesh || !Mesh->GetStaticMeshAsset())
	{
		SectionDraws.clear();
		return;
	}

	const auto& Sections = Mesh->GetStaticMeshAsset()->Sections;
	const auto& Slots = Mesh->GetStaticMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();

	SectionDraws.clear();
	SectionDraws.reserve(Sections.size());

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

		SectionDraws.push_back(Draw);
	}
}
