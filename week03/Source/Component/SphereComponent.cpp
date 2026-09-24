#include "SphereComponent.h"
#include "EngineCore/ResourceManager.h"
#include "LineBatcher.h"

USphereComp::USphereComp()
{
	type = EPrimitiveType::Sphere;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Sphere");
	UpdateAABB();
}

void USphereComp::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}

USphereComp::~USphereComp()
{
}

REGISTER_CLASS(USphereComp);
