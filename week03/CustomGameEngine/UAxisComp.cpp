#include "UAxisComp.h"
#include "URender.h"
#include "ResourceManager.h"
UAxisComp::UAxisComp()
{
	type = EPrimitiveType::Axis;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Axis");
}

UAxisComp::~UAxisComp()
{
}


void UAxisComp::Update(float DeltaTime)
{
}

REGISTER_CLASS(UAxisComp);