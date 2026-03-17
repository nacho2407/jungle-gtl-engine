#include "CubeComponent.h"
#include "ResourceManager.h"

UCubeComp::UCubeComp()
{
	type = EPrimitiveType::Cube;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Cube");
}

UCubeComp::~UCubeComp()
{
}

void UCubeComp::Update(float DeltaTime)
{
}

REGISTER_CLASS(UCubeComp);
