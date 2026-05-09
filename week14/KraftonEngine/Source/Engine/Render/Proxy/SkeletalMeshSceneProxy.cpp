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

	TArray<FMatrix> SkinMatrices;
	SkinMatrices.resize(Asset->Bones.size(), FMatrix::Identity);

	const float Time = (GEngine && GEngine->GetTimer())
		? static_cast<float>(GEngine->GetTimer()->GetTotalTime())
		: 0.0f;

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
	{
		const FBone& Bone = Asset->Bones[BoneIndex];
		const bool bIsTailRoot = Bone.Name == "Tail1" || Bone.Name == "tail1";
		const bool bIsChestRoot = Bone.Name == "Chest" || Bone.Name == "chest";
		const bool bIsLeftArmRoot = Bone.Name == "Larm1" || Bone.Name == "larm1" || Bone.Name == "LArm1" || Bone.Name == "lArm1";
		if (!bIsTailRoot && !bIsChestRoot && !bIsLeftArmRoot) continue;

		const int32 RootBoneIndex = BoneIndex;
		float Angle = sinf(Time * 3.0f) * 0.9f;
		if (bIsChestRoot)
		{
			Angle = sinf(Time * 2.0f) * 0.35f;
		}
		else if (bIsLeftArmRoot)
		{
			Angle = sinf(Time * 4.0f) * 0.65f;
		}
		const FVector Pivot = Bone.GlobalMatrix.GetLocation();

		const FMatrix TranslateToPivot = FMatrix::MakeTranslationMatrix(Pivot * -1.0f);
		const FMatrix Rotate = FMatrix::MakeRotationZ(Angle);
		const FMatrix TranslateBack = FMatrix::MakeTranslationMatrix(Pivot);
		const FMatrix TestDelta = TranslateToPivot * Rotate * TranslateBack;

		for (int32 AffectedBoneIndex = 0; AffectedBoneIndex < static_cast<int32>(Asset->Bones.size()); ++AffectedBoneIndex)
		{
			int32 CurrentBoneIndex = AffectedBoneIndex;
			while (CurrentBoneIndex >= 0 && CurrentBoneIndex < static_cast<int32>(Asset->Bones.size()))
			{
				if (CurrentBoneIndex == RootBoneIndex)
				{
					SkinMatrices[AffectedBoneIndex] = SkinMatrices[AffectedBoneIndex] * TestDelta;
					break;
				}

				CurrentBoneIndex = Asset->Bones[CurrentBoneIndex].ParentIndex;
			}
		}
	}

	for (int32 i = 0; i < (int32)Asset->Vertices.size(); ++i)
	{
		const FVertexPNCTBW& Src = Asset->Vertices[i];
		FVertexPNCTT& Dst = SkinnedVertices[i];

		FVector SkinnedPos(0, 0, 0);
		FVector SkinnedNormal(0, 0, 0);
		float AccumWeight = 0.0f;

		for (int k = 0; k < 4; ++k)
		{
			const int BoneIndex = Src.BoneIndices[k];
			const float Weight = Src.BoneWeights[k];

			if (Weight <= 0.0f) continue;
			if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkinMatrices.size())) continue;

			const FMatrix& M = SkinMatrices[BoneIndex];

			SkinnedPos += M.TransformPositionWithW(Src.Position) * Weight;
			SkinnedNormal += M.TransformVector(Src.Normal) * Weight;
			AccumWeight += Weight;
		}

		if (AccumWeight <= 0.0f)
		{
			SkinnedPos = Src.Position;
			SkinnedNormal = Src.Normal;
		}
		else if (SkinnedNormal.IsNearlyZero())
		{
			SkinnedNormal = Src.Normal;
		}
		else
		{
			SkinnedNormal.Normalize();
		}

		Dst.Position = SkinnedPos;
		Dst.Normal = SkinnedNormal;
		Dst.Color = Src.Color;
		Dst.UV = Src.UV;
		Dst.Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);
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
