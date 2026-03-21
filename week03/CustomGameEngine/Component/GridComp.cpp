#include "GridComp.h"
#include "World.h"
#include "Renderer/Level.h"
#include "Renderer/Renderer.h"
#include "EngineCore/ResourceManager.h"
#include "ImGui/imgui.h"

UGridComp::UGridComp()
{
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Grid");
}

void UGridComp::Update(float deltaTime)
{
}

RenderObject* UGridComp::CreateRenderObject()
{
	RenderObj = UPrimitiveComponent::CreateRenderObject();

	if (RenderObj == nullptr)
		return nullptr;

	RenderObj->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Pickable = false;
	RenderObj->bBatch = true;

	return RenderObj;
}

REGISTER_CLASS(UGridComp);
