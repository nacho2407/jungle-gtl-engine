#include "AxisComponent.h"
#include "Renderer/Renderer.h"
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

RenderObject* UAxisComp::CreateRenderObject()
{
	RenderObj = UPrimitiveComponent::CreateRenderObject();

	if (RenderObj == nullptr)
		return nullptr;

	RenderObj->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Pickable = false;

	return RenderObj;
}

REGISTER_CLASS(UAxisComp);