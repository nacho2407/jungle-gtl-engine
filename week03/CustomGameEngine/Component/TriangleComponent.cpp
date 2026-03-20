#include "TriangleComponent.h"
#include "EngineCore/ResourceManager.h"

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
