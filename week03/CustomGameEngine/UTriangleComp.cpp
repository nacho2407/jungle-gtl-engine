#include "UTriangleComp.h"
#include "URender.h"
#include "ResourceManager.h"

UTriangleComp::UTriangleComp()
{
	type = EPrimitiveType::Triangle;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Triangle");
}

UTriangleComp::~UTriangleComp()
{
}

REGISTER_CLASS(UTriangleComp);
