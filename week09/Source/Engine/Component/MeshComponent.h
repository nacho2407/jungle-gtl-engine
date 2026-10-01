// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/PrimitiveComponent.h"

// UMeshComponent 컴포넌트이다.
class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent() = default;
    ~UMeshComponent() override = default;
};
