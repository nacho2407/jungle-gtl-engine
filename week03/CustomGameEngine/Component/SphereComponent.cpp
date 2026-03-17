#include "SphereComponent.h"
#include "ResourceManager.h"

USphereComp::USphereComp()
{
	type = EPrimitiveType::Sphere;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Sphere");
}

USphereComp::~USphereComp()
{
}

REGISTER_CLASS(USphereComp);
