#include "UUIDBillboardComponent.h"
#include "Actor/Actor.h"
#include "Object/Class.h"
#include <limits>

IMPLEMENT_RTTI(UUUIDBillboardComponent, UTextComponent)

void UUUIDBillboardComponent::PostConstruct()
{
	UTextComponent::PostConstruct();
	SetBillboard(true);
	bDrawDebugBounds = false;
	SetTextScale(0.3f); // UUID 빌보드의 기본 스케일 설정
}

void UUUIDBillboardComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UTextComponent::DuplicateShallow(DuplicatedObject, Context);

	UUUIDBillboardComponent* DuplicatedUUIDBillboardComponent = static_cast<UUUIDBillboardComponent*>(DuplicatedObject);
	DuplicatedUUIDBillboardComponent->WorldOffset = WorldOffset;
}

FString UUUIDBillboardComponent::GetDisplayText() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return "";
	}

	return FString("UUID: ") + OwnerActor->GetUUIDString();
}

FVector UUUIDBillboardComponent::GetRenderWorldPosition() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return WorldOffset;

	USceneComponent* Root = OwnerActor->GetRootComponent();
	if (!Root) return WorldOffset;

	const FVector RootLocation = Root->GetWorldLocation();

	bool bFoundPrimitiveBounds = false;
	float MaxTopZ = -std::numeric_limits<float>::infinity();

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		// 자기 자신(UUID 컴포넌트)이거나 nullptr이면 패스
		if (!Component || Component == this) continue;

		// ⭐ 구형/신형 구분할 필요 없이 UPrimitiveComponent 하나로 통일!
		if (!Component->IsA(UPrimitiveComponent::StaticClass())) continue;

		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
		FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();

		const float TopZ = Bounds.Center.Z + Bounds.BoxExtent.Z;

		if (!bFoundPrimitiveBounds || TopZ > MaxTopZ)
		{
			MaxTopZ = TopZ;
			bFoundPrimitiveBounds = true;
		}
	}

	if (bFoundPrimitiveBounds)
	{
		return FVector(
			RootLocation.X + WorldOffset.X,
			RootLocation.Y + WorldOffset.Y,
			MaxTopZ + WorldOffset.Z
		);
	}

	return RootLocation + WorldOffset;
}

FVector UUUIDBillboardComponent::GetRenderWorldScale() const
{
	// 빌보드는 트랜스포메이션의 스케일과 상관없이 TextScale 만을 절대적으로 사용하는 것이 일반적임
	return FVector(TextScale, TextScale, TextScale);
}

FBoxSphereBounds UUUIDBillboardComponent::GetWorldBounds() const
{
	const FVector Center = GetRenderWorldPosition();
	// radius 를 정사각형 extent 에 맞게 줄이기
	const FVector Extent(TextScale * 3.0f * 0.707f, TextScale * 3.0f * 0.707f, TextScale * 3.0f * 0.707f);

	return { Center, Extent.Size(), Extent };
}
