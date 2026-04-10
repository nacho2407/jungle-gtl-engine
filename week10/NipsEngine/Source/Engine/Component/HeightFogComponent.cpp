#include "HeightFogComponent.h"
#include "Object/Object.h"


DEFINE_CLASS(UHeightFogComponent, UPrimitiveComponent)
REGISTER_FACTORY(UHeightFogComponent)

UHeightFogComponent::UHeightFogComponent() 
{
}

UHeightFogComponent* UHeightFogComponent::Duplicate() 
{
    UHeightFogComponent* NewComp = UObjectManager::Get().CreateObject<UHeightFogComponent>();

	NewComp->FogDensity = FogDensity;
    NewComp->HeightFalloff = HeightFalloff;
    NewComp->FogInscatteringColor = FogInscatteringColor;

	return NewComp;
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps); // 부모 체인 확인 후 맞게 수정
    OutProps.push_back({"FogDensity", EPropertyType::Float, &FogDensity, 0.0f, 1.0f, 0.01f});
    OutProps.push_back({"HeightFalloff", EPropertyType::Float, &HeightFalloff, 0.0f, 10.0f, 0.01f});
    OutProps.push_back({"FogInscatteringColor", EPropertyType::Vec4, &FogInscatteringColor});
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
    // 지금은 별도 후처리 없이 값만 바꿔도 되지만
    // 나중에 셰이더 업데이트 등 필요하면 여기서 처리
}

void UHeightFogComponent::UpdateWorldAABB() const
{
    /** 아예 Bounding Box 가 없으면 Collect 자체가 안돼서 임의로 지정 */
    FAABB AABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
    WorldAABB = AABB;
}

bool UHeightFogComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) 
{ 
	return false; 
}
