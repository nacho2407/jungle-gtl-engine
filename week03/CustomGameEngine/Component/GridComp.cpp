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
	RenderObj->Material = &ResourceManager::GetInstance()->GetMaterial(L"Shader/ShaderLine.hlsl");
	if (RenderObj == nullptr)
		return nullptr;

	RenderObj->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Pickable = false;
	RenderObj->ShowFlag = EEngineShowFlags::SF_EditorPrimitives;
	RenderObj->bBatch = true;
	RenderObj->bDepthEnabled = true; // 깊이 판정 활성화

	return RenderObj;
}

REGISTER_CLASS(UGridComp);
