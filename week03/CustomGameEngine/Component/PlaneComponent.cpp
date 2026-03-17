#include "PlaneComponent.h"
#include "ResourceManager.h"

UPlaneComp::UPlaneComp()
{
	type = EPrimitiveType::Plane;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Plane");
}

UPlaneComp::~UPlaneComp()
{
}

REGISTER_CLASS(UPlaneComp);
