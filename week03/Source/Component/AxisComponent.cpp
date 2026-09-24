#include "AxisComponent.h"
#include "Renderer/Renderer.h"
#include "EngineCore/ResourceManager.h"

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
	RenderObj->Material = &ResourceManager::GetInstance()->GetMaterial(L"Shaders/ShaderLine.hlsl");
	if (RenderObj == nullptr)
		return nullptr;

	RenderObj->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Pickable = false;
	RenderObj->bBatch = true;
	RenderObj->World = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.00f); // Z축으로 약간 이동하여 격자와 겹치지 않도록 함
	RenderObj->ShowFlag = EEngineShowFlags::SF_EditorPrimitives;
	RenderObj->bDepthEnabled = false;

	return RenderObj;
}

REGISTER_CLASS(UAxisComp);