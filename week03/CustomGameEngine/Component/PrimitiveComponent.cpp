#include "PrimitiveComponent.h"
#include "Renderer/Renderer.h"
#include "ResourceManager.h"

UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	if (RenderObj != nullptr)
		delete RenderObj;
}

EPrimitiveType UPrimitiveComponent::GetType()
{
	return type;
}

void UPrimitiveComponent::SetType(EPrimitiveType type)
{
	this->type = type;
}

void UPrimitiveComponent::SetRelativeLocation(const FVector& newLocation)
{
	USceneComponent::SetRelativeLocation(newLocation);
	MarkRenderStateDirty();
}

void UPrimitiveComponent::SetRelativeRotation(const FRotator& newRotation)
{
	USceneComponent::SetRelativeRotation(newRotation);
	MarkRenderStateDirty();
}

void UPrimitiveComponent::SetRelativeScale3D(const FVector& newScale)
{
	USceneComponent::SetRelativeScale3D(newScale);
	MarkRenderStateDirty();
}

void UPrimitiveComponent::OnComponentAdded()
{
	auto RenderObj = CreateRenderObject();
	if (RenderObj != nullptr)
		FLevel::GetInstance().RegisterRenderObject(RenderObj);
}

void UPrimitiveComponent::Update(float DeltaTime)
{

}

RenderObject* UPrimitiveComponent::CreateRenderObject()
{
	if (VResource == nullptr)
		return nullptr;

	RenderObj = new RenderObject();
	ResourceManager* RM = ResourceManager::GetInstance();
	RenderObj->Geometry = this->VResource;
	RenderObj->Material = &RM->GetMaterial(L"Shader/ShaderW0.hlsl");
	UpdateRenderObject();
	return RenderObj;
}

void UPrimitiveComponent::UpdateRenderObject()
{
	RenderObj->World = GetRelativeMatrix();
	RenderObj->bIsSelected = bIsSelected;
}

RenderObject* UPrimitiveComponent::GetRenderObject()
{
	return RenderObj;
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	if (RenderObj != nullptr)
		RenderObj->bIsDirty = true;
}

void UPrimitiveComponent::ResolveRenderStateDirty()
{
	UpdateRenderObject();
	RenderObj->bIsDirty = false;
}

json::JSON UPrimitiveComponent::Serialize()
{
	json::JSON jsonObj = USceneComponent::Serialize();
	switch (type)
	{
	case EPrimitiveType::Sphere:
		jsonObj["Type"] = "Sphere";
		break;
	case EPrimitiveType::Cube:
		jsonObj["Type"] = "Cube";
		break;
	case EPrimitiveType::Triangle:
		jsonObj["Type"] = "Triangle";
		break;
	case EPrimitiveType::Plane:
		jsonObj["Type"] = "Plane";
		break;
	}
	return jsonObj;
}

void UPrimitiveComponent::Deserialize(json::JSON jsonObj)
{
	USceneComponent::Deserialize(jsonObj);

	if (jsonObj["Type"].ToString() == "Sphere")
		type = EPrimitiveType::Sphere;
	else if (jsonObj["Type"].ToString() == "Cube")
		type = EPrimitiveType::Cube;
	else if (jsonObj["Type"].ToString() == "Triangle")
		type = EPrimitiveType::Triangle;
	else if (jsonObj["Type"].ToString() == "Plane")
		type = EPrimitiveType::Plane;
}



REGISTER_CLASS(UPrimitiveComponent);
