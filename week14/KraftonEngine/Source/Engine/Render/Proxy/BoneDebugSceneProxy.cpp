#include "BoneDebugSceneProxy.h"

#include "Component/BoneDebugComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"

#pragma region Line Draw

static void AddLine(TArray<FWireLine>& Lines, const FVector& A, const FVector& B)
{
	Lines.push_back({ A, B });
}

static void AddWireCircle(TArray<FWireLine>& Lines, const FVector& Center, const FVector& AxisA, const FVector& AxisB,
	float Radius, int32 Segments)
{
	if (Radius <= 0.0f || Segments < 3) return;

	const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
	FVector Prev = Center + AxisA * Radius;

	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = Step * i;
		FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
		AddLine(Lines, Prev, Next);
		Prev = Next;
	}
}

static void BuildLowSphereLines(TArray<FWireLine>& Lines, const FVector& Center, float Radius)
{
	constexpr int32 Segments = 8;

	const FVector AxisA(1.0f, 0.0f, 0.0f);
	const FVector AxisB(0.0f, 1.0f, 0.0f);
	const FVector AxisC(0.0f, 0.0f, 1.0f);
	AddWireCircle(Lines, Center, AxisA, AxisB, Radius, 12);
	AddWireCircle(Lines, Center, AxisB, AxisC, Radius, 12);
	AddWireCircle(Lines, Center, AxisC, AxisA, Radius, 12);
}

static void BuildBonePyramidLines(TArray<FWireLine>& Lines, const FVector& Start, const FVector& End, float WidthScale)
{
	FVector BoneVector = End - Start;
	const float Length = BoneVector.Length();

	if (Length <= 0.001f) return;

	const FVector Dir = BoneVector / Length;

	FVector UpHint = std::fabs(Dir.Z) > 0.9f ? FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);
	FVector Right = UpHint.Cross(Dir).Normalized();
	FVector Up = Dir.Cross(Right).Normalized();

	const float HalfWidth = Length * WidthScale;

	const FVector Center = End;

	const FVector C0 = Center + Right * HalfWidth + Up * HalfWidth;
	const FVector C1 = Center - Right * HalfWidth + Up * HalfWidth;
	const FVector C2 = Center - Right * HalfWidth - Up * HalfWidth;
	const FVector C3 = Center + Right * HalfWidth - Up * HalfWidth;

	AddLine(Lines, Start, C0);
	AddLine(Lines, Start, C1);
	AddLine(Lines, Start, C2);
	AddLine(Lines, Start, C3);

	AddLine(Lines, C0, C1);
	AddLine(Lines, C1, C2);
	AddLine(Lines, C2, C3);
	AddLine(Lines, C3, C0);
}

#pragma endregion


FBoneDebugSceneProxy::FBoneDebugSceneProxy(UBoneDebugComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags = EPrimitiveProxyFlags::EditorOnly
		| EPrimitiveProxyFlags::NeverCull
		| EPrimitiveProxyFlags::BoneDebug;

	BoneColor = FVector4(0.49f, 0.91f, 0.48f, 1.0f);
	ParentBoneColor = FVector4(0.93f, 0.69f, 0.38f, 1.0f);
	RebuildLines();
}

FBoneDebugSceneProxy::~FBoneDebugSceneProxy()
{
}

void FBoneDebugSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	RebuildLines();
}

void FBoneDebugSceneProxy::RebuildLines()
{
	CachedLines.clear();
	CachedParentBoneLines.clear();

	UBoneDebugComponent* Comp = static_cast<UBoneDebugComponent*>(GetOwner());
	if (!Comp) return;

	USkeletalMeshComponent* MeshComp = Comp->GetTargetMeshComponent();
	const int32 BoneIndex = Comp->GetSelectedBoneIndex();
	if (!MeshComp || BoneIndex < 0) return;

	USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex >= static_cast<int32>(Asset->Bones.size())) return;

	const FBoundingBox Bounds = MeshComp->GetWorldBoundingBox();
	const FVector Extent = Bounds.GetExtent();
	const float ModelSize = Extent.Length();

	const float JointRadius = ModelSize * 0.01f;
	const float PyramidWidthScale = 0.03f;

	const FVector BonePos = MeshComp->GetBoneLocationByIndex(BoneIndex);

	BuildLowSphereLines(CachedLines, BonePos, JointRadius);

	for (int32 i = 0; i < static_cast<int32>(Asset->Bones.size()); ++i)
	{
		if (Asset->Bones[i].ParentIndex == BoneIndex)
		{
			const FVector ChildPos = MeshComp->GetBoneLocationByIndex(i);
			BuildBonePyramidLines(CachedLines, ChildPos, BonePos, PyramidWidthScale);
		}
	}

	if (Asset->Bones[BoneIndex].ParentIndex != -1)
	{
		const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
		const FVector ParentPos = MeshComp->GetBoneLocationByIndex(ParentIndex);
		BuildBonePyramidLines(CachedParentBoneLines, BonePos, ParentPos, PyramidWidthScale);
	}
}
