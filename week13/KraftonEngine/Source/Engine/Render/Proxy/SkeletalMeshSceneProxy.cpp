#include "SkeletalMeshSceneProxy.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Geometry/CollisionDebugGeometry.h"
#include "Render/Types/FrameContext.h"
#include "Runtime/Engine.h"
#include "Profiling/Time/Timer.h"
#include "Profiling/Stats/Stats.h"
#include "Object/Object.h"

#include <algorithm>
#include <cstring>

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::SkeletalMesh;
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
{
	ReleaseSkinMatrixBuffer();
}   

USkeletalMeshComponent* FSkeletalMeshSceneProxy::GetSkeletalMeshComponent() const
{
	return Cast<USkeletalMeshComponent>(GetOwner());
}

bool FSkeletalMeshSceneProxy::GetPhysicsAssetBoneWorldTransform(const FName& BoneName, FTransform& OutBoneWorldTM) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SMC || !Asset)
	{
		return false;
	}

	const FString BoneNameString = BoneName.ToString();
	if (BoneNameString.empty())
	{
		return false;
	}

	int32 BoneIndex = -1;
	for (int32 Index = 0; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].Name == BoneNameString)
		{
			BoneIndex = Index;
			break;
		}
	}

	if (BoneIndex < 0)
	{
		return false;
	}

	TArray<FTransform> ComponentSpaceBoneTransforms;
	SMC->GetCurrentBoneGlobalTransforms(ComponentSpaceBoneTransforms);
	if (BoneIndex >= static_cast<int32>(ComponentSpaceBoneTransforms.size()))
	{
		return false;
	}

	const FTransform ComponentToWorldTM(SMC->GetWorldMatrix());
	OutBoneWorldTM = ComponentSpaceBoneTransforms[BoneIndex] * ComponentToWorldTM;
	return true;
}

void FSkeletalMeshSceneProxy::BuildPhysicsAssetWireLines(const FFrameContext& Frame, TArray<FWireLine>& OutLines) const
{
	if (!Frame.RenderOptions.ShowFlags.bDebugPhysicsAsset)
	{
		return;
	}

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* SkelMesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	if (!SkelMesh)
	{
		return;
	}

	const UPhysicsAsset* PhysicsAsset = SkelMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	for (const UBodySetup* BodySetup : PhysicsAsset->GetBodySetups())
	{
		if (!BodySetup)
		{
			continue;
		}

		FTransform BoneWorldTM;
		if (!GetPhysicsAssetBoneWorldTransform(BodySetup->BoneName, BoneWorldTM))
		{
			continue;
		}

		const FVector BoneScale3D = BoneWorldTM.Scale;
		const float UniformScale = BoneScale3D.GetAbsMax();
		BoneWorldTM.Scale = FVector::OneVector;

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
		{
			const FVector WorldCenter = BoneWorldTM.TransformPosition(SphereElem.Center * UniformScale);
			const float WorldRadius = SphereElem.Radius * UniformScale;
			FCollisionDebugGeometry::AddWireSphere(OutLines, WorldCenter, WorldRadius);
		}

		for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
		{
			FTransform ShapeLocalTM(BoxElem.Center * UniformScale, BoxElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			const FVector HalfExtent(
				BoxElem.X * 0.5f * UniformScale,
				BoxElem.Y * 0.5f * UniformScale,
				BoxElem.Z * 0.5f * UniformScale);
			FCollisionDebugGeometry::AddWireBox(OutLines, ShapeWorldTM, HalfExtent);
		}

		for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
		{
			FTransform ShapeLocalTM(SphylElem.Center * UniformScale, SphylElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			FCollisionDebugGeometry::AddWireCapsule(
				OutLines,
				ShapeWorldTM,
				SphylElem.Radius * UniformScale,
				SphylElem.Length * UniformScale);
		}

		for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
		{
			FTransform ShapeWorldTM = ConvexElem.GetTransform() * BoneWorldTM;
			ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
			FCollisionDebugGeometry::AddWireConvex(OutLines, ConvexElem, ShapeWorldTM);
		}
	}
}

void FSkeletalMeshSceneProxy::BuildPhysicsAssetSolidMesh(const FFrameContext& Frame, FPhysicsDebugSolidMesh& OutMesh) const
{
	OutMesh.Reset();

	if (!Frame.RenderOptions.ShowFlags.bDebugPhysicsAsset)
	{
		return;
	}

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* SkelMesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	if (!SkelMesh)
	{
		return;
	}

	const UPhysicsAsset* PhysicsAsset = SkelMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	const FVector4 SolidColor(0.1f, 0.7f, 1.0f, 0.22f);

	for (const UBodySetup* BodySetup : PhysicsAsset->GetBodySetups())
	{
		if (!BodySetup)
		{
			continue;
		}

		FTransform BoneWorldTM;
		if (!GetPhysicsAssetBoneWorldTransform(BodySetup->BoneName, BoneWorldTM))
		{
			continue;
		}

		const FVector BoneScale3D = BoneWorldTM.Scale;
		const float UniformScale = BoneScale3D.GetAbsMax();
		BoneWorldTM.Scale = FVector::OneVector;

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
		{
			const FVector WorldCenter = BoneWorldTM.TransformPosition(SphereElem.Center * UniformScale);
			const FTransform SphereWorldTM(WorldCenter, BoneWorldTM.Rotation, FVector::OneVector);
			FCollisionDebugGeometry::AddSolidSphere(OutMesh, SphereWorldTM, SphereElem.Radius * UniformScale, SolidColor);
		}

		for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
		{
			FTransform ShapeLocalTM(BoxElem.Center * UniformScale, BoxElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			const FVector HalfExtent(
				BoxElem.X * 0.5f * UniformScale,
				BoxElem.Y * 0.5f * UniformScale,
				BoxElem.Z * 0.5f * UniformScale);
			FCollisionDebugGeometry::AddSolidBox(OutMesh, ShapeWorldTM, HalfExtent, SolidColor);
		}

		for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
		{
			FTransform ShapeLocalTM(SphylElem.Center * UniformScale, SphylElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			FCollisionDebugGeometry::AddSolidCapsule(
				OutMesh,
				ShapeWorldTM,
				SphylElem.Radius * UniformScale,
				SphylElem.Length * UniformScale,
				SolidColor);
		}

		for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
		{
			FTransform ShapeWorldTM = ConvexElem.GetTransform() * BoneWorldTM;
			ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
			FCollisionDebugGeometry::AddSolidConvex(OutMesh, ConvexElem, ShapeWorldTM, SolidColor);
		}
	}
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
};

void FSkeletalMeshSceneProxy::UpdateMesh()
{
	if (!HasValidOwner())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		bVisible = false;
		return;
	}

	UPrimitiveComponent* OwnerComp = GetOwner();
	MeshBuffer = IsValid(OwnerComp) ? OwnerComp->GetMeshBuffer() : nullptr;
	RebuildSectionDraws();

	CachedDynamicVertexCount = 0;
	UploadedSkinnedRevision = 0;
	UploadedSkinMatrixRevision = 0;
	bDynamicBufferNeedsCreate = true;
	ReleaseSkinMatrixBuffer();

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (Asset)
	{
		CachedDynamicVertexCount = static_cast<uint32>(Asset->Vertices.size());
	}
}

bool FSkeletalMeshSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!IsValid(SMC)) return false;

	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || !Asset->RenderBuffer || !Asset->RenderBuffer->IsValid()) return false;

	const TArray<FVertexPNCTT>& SkinnedVertices = SMC->GetSkinnedVertices();
	const uint32 VertexCount = static_cast<uint32>(SkinnedVertices.size());
	if (VertexCount == 0) return false;

	if (bDynamicBufferNeedsCreate || !DynamicVertexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, CachedDynamicVertexCount ? CachedDynamicVertexCount : VertexCount, sizeof(FVertexPNCTT));
		bDynamicBufferNeedsCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);

	const uint64 CurrentRevision = SMC->GetSkinnedRevision();
	if (UploadedSkinnedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, SkinnedVertices.data(), VertexCount))
		{
			return false;
		}
		UploadedSkinnedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

bool FSkeletalMeshSceneProxy::PrepareGpuSkinningDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || !Asset->RenderBuffer || !Asset->RenderBuffer->IsValid()) return false;

	if (!UpdateSkinMatrixBuffer(Device, Context)) return false;

	OutBuffer = {};
	OutBuffer.VB = Asset->RenderBuffer->GetVertexBuffer().GetBuffer();
	OutBuffer.VBStride = Asset->RenderBuffer->GetVertexBuffer().GetStride();
	OutBuffer.IB = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

ID3D11ShaderResourceView* FSkeletalMeshSceneProxy::GetSkinMatrixSRV(ID3D11Device* Device, ID3D11DeviceContext* Context) const
{
	UpdateSkinMatrixBuffer(Device, Context);
	return SkinMatrixSRV;
}

void FSkeletalMeshSceneProxy::ReleaseSkinMatrixBuffer() const
{
	if (SkinMatrixSRV)
	{
		SkinMatrixSRV->Release();
		SkinMatrixSRV = nullptr;
	}

	if (SkinMatrixBuffer)
	{
		SkinMatrixBuffer->Release();
		SkinMatrixBuffer = nullptr;
	}

	SkinMatrixCapacity = 0;
}

bool FSkeletalMeshSceneProxy::UpdateSkinMatrixBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Device || !Context || !SMC || !Asset || Asset->Bones.empty()) return false;

	const uint32 MatrixCount = static_cast<uint32>(Asset->Bones.size());
	const uint64 CurrentRevision = SMC->GetSkinnedRevision();

	if (!SkinMatrixBuffer || !SkinMatrixSRV || SkinMatrixCapacity < MatrixCount)
	{
		ReleaseSkinMatrixBuffer();

		D3D11_BUFFER_DESC BufferDesc = {};
		BufferDesc.ByteWidth = sizeof(FMatrix) * MatrixCount;
		BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = sizeof(FMatrix);

		if (FAILED(Device->CreateBuffer(&BufferDesc, nullptr, &SkinMatrixBuffer)))
		{
			ReleaseSkinMatrixBuffer();
			return false;
		}

		SkinMatrixBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen("SkinMatrixBuffer")), "SkinMatrixBuffer");

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = MatrixCount;

		if (FAILED(Device->CreateShaderResourceView(SkinMatrixBuffer, &SRVDesc, &SkinMatrixSRV)))
		{
			ReleaseSkinMatrixBuffer();
			return false;
		}

		SkinMatrixSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen("SkinMatrixSRV")), "SkinMatrixSRV");
		SkinMatrixCapacity = MatrixCount;
		UploadedSkinMatrixRevision = 0;
	}

	if (UploadedSkinMatrixRevision == CurrentRevision)
	{
		return true;
	}

	TArray<FMatrix> SkinMatrices;
	SMC->BuildSkinMatrices(SkinMatrices);
	if (SkinMatrices.size() != MatrixCount) return false;

	{
		SCOPE_STAT_CAT("GPUSkinning_MatrixUpload", "Skinning");

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(Context->Map(SkinMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			return false;
		}

		std::memcpy(Mapped.pData, SkinMatrices.data(), sizeof(FMatrix) * MatrixCount);
		Context->Unmap(SkinMatrixBuffer, 0);
	}

	UploadedSkinMatrixRevision = CurrentRevision;
	return true;
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!IsValid(SMC))
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		return;
	}
	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	SectionDraws.clear();

	const auto& Slots = Mesh->GetSkeletalMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();

	for (const FSkeletalMeshSection& Section : Mesh->GetSkeletalMeshAsset()->Sections)
	{
		FMeshSectionDraw Draw;
		Draw.Material = nullptr;
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

		if (!Draw.Material)
		{
			Draw.Material = FMaterialManager::Get().GetOrCreateMaterial("None");
		}

		SectionDraws.push_back(Draw);
	}
}
