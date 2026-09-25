#pragma once
#include "Core/EngineAPI.h"
#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    class UStaticMeshComponent;
}

/**
 * @brief 정적 메시를 월드에 배치하기 위한 전용 액터 클래스입니다.
 */
class ENGINE_API AStaticMeshActor : public AActor
{
  public:
    DECLARE_RTTI(AStaticMeshActor, AActor)

    AStaticMeshActor();
    virtual ~AStaticMeshActor() override;

    Engine::Component::UStaticMeshComponent* GetStaticMeshComponent() const
    {
        return StaticMeshComponent;
    }

    bool           IsRenderable() const override;
    bool           IsSelected() const override;
    uint32         GetObjectId() const override;

  private:
    Engine::Component::UStaticMeshComponent* StaticMeshComponent = nullptr;
};
