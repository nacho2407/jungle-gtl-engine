#include "UCubeComp.h"
#include "URender.h"
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
	/*float Speed = 10.f;
	RelativeRotation.y += DeltaTime * Speed;*/
}

REGISTER_CLASS(UCubeComp);
