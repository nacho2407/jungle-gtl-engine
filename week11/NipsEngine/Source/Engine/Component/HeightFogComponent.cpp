#include "HeightFogComponent.h"
#include "Object/Object.h"


DEFINE_CLASS(UHeightFogComponent, UPrimitiveComponent)
REGISTER_FACTORY(UHeightFogComponent)

UHeightFogComponent::UHeightFogComponent() 
{
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps); // ActorComp + SceneComp + Visible
    OutProps.push_back({"FogDensity", EPropertyType::Float, &FogDensity, 0.0f, 1.0f, 0.01f});
    OutProps.push_back({"HeightFalloff", EPropertyType::Float, &HeightFalloff, 0.0f, 10.0f, 0.01f});
    OutProps.push_back({"FogInscatteringColor", EPropertyType::Vec4, &FogInscatteringColor});
    OutProps.push_back({"FogHeight", EPropertyType::Float, &FogHeight});
    OutProps.push_back({"FogStartDistance", EPropertyType::Float, &FogStartDistance, 0.0f});
    OutProps.push_back({"FogCutoffDistance", EPropertyType::Float, &FogCutoffDistance});
    OutProps.push_back({"FogMaxOpacity", EPropertyType::Float, &FogMaxOpacity, 0.0f, 1.0f, 0.01f});
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
    // 지금은 별도 후처리 없이 값만 바꿔도 되지만
    // 나중에 셰이더 업데이트 등 필요하면 여기서 처리
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
