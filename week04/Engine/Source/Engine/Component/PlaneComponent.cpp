#include "PlaneComponent.h"

namespace Engine::Component
{
    void UPlaneComp::Update(float DeltaTime)
    {
        UPrimitiveComponent::Update(DeltaTime);
    }

    REGISTER_CLASS(Engine::Component, UPlaneComp);
}
