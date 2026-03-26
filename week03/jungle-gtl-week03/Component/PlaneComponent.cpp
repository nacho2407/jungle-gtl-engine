#include "PlaneComponent.h"
#include "EngineCore/ResourceManager.h"
#include "LineBatcher.h"
UPlaneComp::UPlaneComp()
{
	type = EPrimitiveType::Plane;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Plane");
	UpdateAABB();
}

UPlaneComp::~UPlaneComp()
{

}

void UPlaneComp::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}



REGISTER_CLASS(UPlaneComp);
