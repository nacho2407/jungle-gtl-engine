#pragma once

#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    class UTriangleComponent;
    class UPrimitiveComponent;
}

/*
    Triangle primitive 하나를 루트 컴포넌트로 들고 있는 가장 기본적인 렌더 가능 액터입니다.
    Scene에 추가하면 UTriangleComponent의 메시/색상/트랜스폼을 그대로 렌더 브리지로 노출합니다.
*/
class ENGINE_API ATriangleActor : public AActor
{
    DECLARE_RTTI(ATriangleActor, AActor)

  public:
    ATriangleActor();
    ~ATriangleActor() override = default;

    Engine::Component::UTriangleComponent* GetTriangleComponent() const;

    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    FColor         GetColor() const override;
    EBasicMeshType GetMeshType() const override;
    uint32         GetObjectId() const override;

private:
    Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};
