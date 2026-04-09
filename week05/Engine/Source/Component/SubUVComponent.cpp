#include "SubUVComponent.h"

#include <algorithm>

#include "Object/Class.h"
#include "Renderer/MeshData.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(USubUVComponent, UPrimitiveComponent)

void USubUVComponent::PostConstruct()
{
	// SubUV 렌더링용 메시 객체 생성
	bDrawDebugBounds = false;
	bCanEverTick = true;
	SubUVMesh = std::make_shared<FDynamicMesh>();
	MarkSubUVMeshDirty();
	ResetAnimation();
}

void USubUVComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();
	ResetAnimation();
}

void USubUVComponent::Tick(float DeltaTime)
{
	UPrimitiveComponent::Tick(DeltaTime);

	const float SafeDeltaTime = (std::max)(0.0f, DeltaTime);
	ElapsedTime += SafeDeltaTime;

	CurrentFrame = ComputeFrameIndex(
		Columns,
		Rows,
		TotalFrames,
		FirstFrame,
		LastFrame,
		FPS,
		ElapsedTime,
		bLoop);
}

void USubUVComponent::ResetAnimation()
{
	ElapsedTime = 0.0f;
	CurrentFrame = ComputeFrameIndex(
		Columns,
		Rows,
		TotalFrames,
		FirstFrame,
		LastFrame,
		FPS,
		0.0f,
		bLoop);
}

void USubUVComponent::MarkSubUVMeshDirty()
{
	bSubUVMeshDirty = true;
	if (SubUVMesh)
	{
		SubUVMesh->bIsDirty = true;
	}
}

int32 USubUVComponent::ComputeFrameIndex(
	int32 InColumns,
	int32 InRows,
	int32 InTotalFrames,
	int32 InFirstFrame,
	int32 InLastFrame,
	float InFPS,
	float InElapsedTime,
	bool bInLoop)
{
	const int32 SafeColumns = (std::max)(1, InColumns);
	const int32 SafeRows = (std::max)(1, InRows);
	const int32 MaxSheetFrames = SafeColumns * SafeRows;
	const int32 SafeTotalFrames = (std::max)(1, (std::min)(InTotalFrames, MaxSheetFrames));

	int32 SafeFirstFrame = (std::max)(0, (std::min)(InFirstFrame, SafeTotalFrames - 1));
	int32 SafeLastFrame = (std::max)(0, (std::min)(InLastFrame, SafeTotalFrames - 1));
	if (SafeFirstFrame > SafeLastFrame)
	{
		std::swap(SafeFirstFrame, SafeLastFrame);
	}

	const int32 PlayableFrameCount = SafeLastFrame - SafeFirstFrame + 1;
	if (PlayableFrameCount <= 0)
	{
		return SafeFirstFrame;
	}

	const float SafeFPS = (std::max)(0.0f, InFPS);
	if (SafeFPS <= 0.0f)
	{
		return SafeFirstFrame;
	}

	const int32 AnimationFrame = static_cast<int32>((std::max)(0.0f, InElapsedTime) * SafeFPS);

	int32 LocalFrameIndex = 0;
	if (bInLoop)
	{
		LocalFrameIndex = AnimationFrame % PlayableFrameCount;
	}
	else
	{
		LocalFrameIndex = (std::min)(AnimationFrame, PlayableFrameCount - 1);
	}

	return SafeFirstFrame + LocalFrameIndex;
}

void USubUVComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UPrimitiveComponent::DuplicateShallow(DuplicatedObject, Context);

	USubUVComponent* DuplicatedSubUVComponent = static_cast<USubUVComponent*>(DuplicatedObject);
	DuplicatedSubUVComponent->Size = Size;
	DuplicatedSubUVComponent->Color = Color;
	DuplicatedSubUVComponent->Columns = Columns;
	DuplicatedSubUVComponent->Rows = Rows;
	DuplicatedSubUVComponent->TotalFrames = TotalFrames;
	DuplicatedSubUVComponent->FirstFrame = FirstFrame;
	DuplicatedSubUVComponent->LastFrame = LastFrame;
	DuplicatedSubUVComponent->FPS = FPS;
	DuplicatedSubUVComponent->bLoop = bLoop;
	DuplicatedSubUVComponent->bBillboard = bBillboard;
	DuplicatedSubUVComponent->ResetAnimation();
	DuplicatedSubUVComponent->MarkSubUVMeshDirty();
}

FRenderMesh* USubUVComponent::GetRenderMesh() const
{
	return SubUVMesh.get();
}

void USubUVComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		Ar.Serialize("Size", Size);
		Ar.Serialize("Color", Color);
		Ar.Serialize("Columns", Columns);
		Ar.Serialize("Rows", Rows);
		Ar.Serialize("TotalFrames", TotalFrames);
		Ar.Serialize("FirstFrame", FirstFrame);
		Ar.Serialize("LastFrame", LastFrame);
		Ar.Serialize("FPS", FPS);
		Ar.Serialize("Loop", bLoop);
		Ar.Serialize("Billboard", bBillboard);
	}
	else
	{
		Ar.Serialize("Size", Size);
		Ar.Serialize("Color", Color);
		Ar.Serialize("Columns", Columns);
		Ar.Serialize("Rows", Rows);
		Ar.Serialize("TotalFrames", TotalFrames);
		Ar.Serialize("FirstFrame", FirstFrame);
		Ar.Serialize("LastFrame", LastFrame);
		Ar.Serialize("FPS", FPS);
		Ar.Serialize("Loop", bLoop);
		Ar.Serialize("Billboard", bBillboard);

		Columns = (std::max)(1, Columns);
		Rows = (std::max)(1, Rows);
		TotalFrames = (std::max)(1, TotalFrames);
		FPS = (std::max)(0.0f, FPS);

		ResetAnimation();
		MarkSubUVMeshDirty();
		UpdateBounds();
	}
}

FBoxSphereBounds USubUVComponent::GetWorldBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfW = Size.X * 0.5f * WorldScale.X;
	const float HalfH = Size.Y * 0.5f * WorldScale.Y;
	const float HalfZ = ((HalfW > HalfH) ? HalfW : HalfH);

	const FVector BoxExtent(HalfW, HalfH, HalfZ);
	return { Center, BoxExtent.Size(), BoxExtent };
}
