#include "Actor.h"
#include "Engine/Component/SceneComponent.h"

AActor::AActor() = default;

bool AActor::IsPickable() const { return bPickable; }

void AActor::SetPickable(bool bInPickable) { bPickable = bInPickable; }

FMatrix AActor::GetWorldMatrix() const
{
    //  현재 설계 상 RootComponent는 PrimitiveComponent로 간주
    if (RootComponent != nullptr)
    {
        return RootComponent->GetRelativeMatrix();
    }

    return FMatrix::Identity;
}

EBasicMeshType AActor::GetMeshType() const
{
    // 기본 Actor는 렌더 대상이 아니므로 의미 없는 기본값
    // 실제 렌더 Actor가 override 하도록 둠
    return EBasicMeshType::Cube;
}