#include "PlaneComponent.h"

Engine::Component::UPlaneComp::UPlaneComp() {

}

Engine::Component::UPlaneComp::~UPlaneComp() {
}

void Engine::Component::UPlaneComp::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}

REGISTER_CLASS(Engine::Component, UPlaneComp);
