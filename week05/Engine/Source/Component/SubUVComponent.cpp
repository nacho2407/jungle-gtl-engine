#include "SubUVComponent.h"
#include "Object/Class.h"
#include "Renderer/MeshData.h"

IMPLEMENT_RTTI(USubUVComponent, UPrimitiveComponent)

void USubUVComponent::PostConstruct()
{
	// SubUV 렌더링용 메시 객체 생성
	bDrawDebugBounds = false;
	SubUVMesh = std::make_shared<FDynamicMesh>();
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
}

FRenderMesh* USubUVComponent::GetRenderMesh() const { return SubUVMesh.get(); }

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
