#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Render/Command/DrawCommand.h"
#include "Runtime/Engine.h"
#include "Profiling/Timer.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
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

bool FSkeletalMeshSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC) return false;

	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return false;

	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (Asset->Vertices.empty() || !Asset->RenderBuffer) return false;

	SkinnedVertices.resize(Asset->Vertices.size());

	TArray<FTransform> BoneGlobals;
	SMC->GetCurrentBoneGlobalTransforms(BoneGlobals);

	auto SkinVertexRange = [&](uint32 VertexStart, uint32 VertexEnd, const FMatrix& MeshBindGlobal)
	{
		TArray<FMatrix> SkinMatrices;
		SkinMatrices.resize(Asset->Bones.size(), FMatrix::Identity);

		for (int32 BoneIndex = 0; BoneIndex < (int32)Asset->Bones.size(); ++BoneIndex)
		{
			if (BoneIndex < static_cast<int32>(BoneGlobals.size()))
			{
				SkinMatrices[BoneIndex] =
					MeshBindGlobal * Asset->Bones[BoneIndex].InverseBindPoseMatrix * BoneGlobals[BoneIndex].ToMatrix();
			}
		}

		VertexEnd = std::min<uint32>(VertexEnd, (uint32)Asset->Vertices.size());
		for (uint32 i = VertexStart; i < VertexEnd; ++i)
		{
			const FVertexPNCTBW& Src = Asset->Vertices[i];
			FVertexPNCTT& Dst = SkinnedVertices[i];

			FVector SkinnedPos = FVector::ZeroVector;
			FVector SkinnedNormal = FVector::ZeroVector;
			float AccumWeight = 0.0f;

			for (int32 k = 0; k < 4; ++k)
			{
				const int32 BoneIndex = Src.BoneIndices[k];
				const float Weight = Src.BoneWeights[k];

				if (Weight <= 0.0f) continue;
				if (BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) continue;

				const FMatrix& M = SkinMatrices[BoneIndex];

				SkinnedPos += M.TransformPositionWithW(Src.Position) * Weight;
				SkinnedNormal += M.TransformVector(Src.Normal) * Weight;
				AccumWeight += Weight;
			}

			if (AccumWeight <= 0.0f)
			{
				SkinnedPos = MeshBindGlobal.TransformPositionWithW(Src.Position);
				SkinnedNormal = MeshBindGlobal.TransformVector(Src.Normal);
				if (!SkinnedNormal.IsNearlyZero())
				{
					SkinnedNormal.Normalize();
				}
			}
			else if (!SkinnedNormal.IsNearlyZero())
			{
				SkinnedNormal.Normalize();
			}

			Dst.Position = SkinnedPos;
			Dst.Normal = SkinnedNormal;
			Dst.Color = Src.Color;
			Dst.UV = Src.UV;
			Dst.Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);
		}
	};

	if (!Asset->MeshRanges.empty())
	{
		for (const FSkeletalMeshRange& Range : Asset->MeshRanges)
		{
			SkinVertexRange(Range.VertexStart, Range.VertexEnd, Range.MeshBindGlobal);
		}
	}
	else
	{
		SkinVertexRange(0, (uint32)Asset->Vertices.size(), FMatrix::Identity);
	}

	const uint32 VertexCount = static_cast<uint32>(SkinnedVertices.size());

	if (!DynamicVertexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, VertexCount, sizeof(FVertexPNCTT));
	}
	
	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);

	if (!DynamicVertexBuffer.Update(Context, SkinnedVertices.data(), VertexCount))
	{
		return false;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();

	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
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
