#include "TriangleComponent.h"
#include "EngineCore/ResourceManager.h"
#include "LineBatcher.h"
UTriangleComp::UTriangleComp()
{
	type = EPrimitiveType::Triangle;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Triangle");
	UpdateAABB();
}

UTriangleComp::~UTriangleComp()
{
}

void UTriangleComp::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}

REGISTER_CLASS(UTriangleComp);
