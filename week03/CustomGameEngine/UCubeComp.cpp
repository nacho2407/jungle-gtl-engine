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
	float Speed = 10.f;
	FVector rot = GetRelativeRotation();
	//rot.y += DeltaTime * Speed;
	SetRelativeRotation(rot);
}

REGISTER_CLASS(UCubeComp);
