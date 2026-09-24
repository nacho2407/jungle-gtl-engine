#include "CubeComponent.h"
#include "EngineCore/ResourceManager.h"
#include "LineBatcher.h"
UCubeComp::UCubeComp()
{
	type = EPrimitiveType::Cube;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Cube");
	UpdateAABB();
}

UCubeComp::~UCubeComp()
{
}


void UCubeComp::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}


REGISTER_CLASS(UCubeComp);
