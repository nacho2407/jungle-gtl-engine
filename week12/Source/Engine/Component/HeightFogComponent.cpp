#include "HeightFogComponent.h"
#include "Object/Object.h"



UHeightFogComponent::UHeightFogComponent() 
{
	bEnableCull = false;
}

void UHeightFogComponent::UpdateWorldAABB() const
{
	// Height Fog는 화면 전역 효과이므로 작은 로컬 박스 대신
	// 월드에서 충분히 큰 AABB를 사용해 수집/공간 인덱스 단계에서 누락되지 않게 한다.
	WorldAABB.Reset();

	const FVector Center = GetWorldLocation();
	const FVector Scale = GetWorldScale();

	const float SafeScaleX = (Scale.X > 0.01f) ? Scale.X : 0.01f;
	const float SafeScaleY = (Scale.Y > 0.01f) ? Scale.Y : 0.01f;
	const float SafeScaleZ = (Scale.Z > 0.01f) ? Scale.Z : 0.01f;

	const float HorizontalExtent = 50000.0f * ((SafeScaleX > SafeScaleY) ? SafeScaleX : SafeScaleY);
	const float VerticalExtent = 20000.0f * SafeScaleZ;

	WorldAABB.Expand(Center - FVector(HorizontalExtent, HorizontalExtent, VerticalExtent));
	WorldAABB.Expand(Center + FVector(HorizontalExtent, HorizontalExtent, VerticalExtent));
}

bool UHeightFogComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) 
{ 
	return false; 
}
