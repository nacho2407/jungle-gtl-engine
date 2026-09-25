#pragma once
#include "Core/EngineAPI.h"
#include "Engine/Game/Actor.h"

namespace Engine::Component
{
    class UQuadComponent;
}

/**
 * @brief 2D 평면(Quad) 메시를 렌더링하기 위한 액터입니다.
 */
class ENGINE_API AQuadActor : public AActor
{
public:
    DECLARE_RTTI(AQuadActor, AActor)

    AQuadActor();
    virtual ~AQuadActor() override;

    Engine::Component::UQuadComponent* GetQuadComponent() const
    {
        return QuadComponent;
    }

private:
    Engine::Component::UQuadComponent* QuadComponent = nullptr;
};
