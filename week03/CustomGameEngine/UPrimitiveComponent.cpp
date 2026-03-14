#include "UPrimitiveComponent.h"
#include "URender.h"
#include "ResourceManager.h"

UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

EPrimitiveType UPrimitiveComponent::GetType()
{
	return type;
}

void UPrimitiveComponent::SetType(EPrimitiveType type)
{
	this->type = type;
}

void UPrimitiveComponent::Update(float DeltaTime)
{

}

URenderObject* UPrimitiveComponent::Render()
{
	if (VResource == nullptr)
		return nullptr;

	UObject* renderObj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* RenderObj = dynamic_cast<URenderObject*>(renderObj);
	ResourceManager* RM = ResourceManager::GetInstance();

	RenderObj->Geometry = this->VResource;
	RenderObj->Material = &RM->GetMaterial(L"ShaderW0.hlsl");
	RenderObj->World = GetRelativeMatrix();

	return RenderObj;
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
}

REGISTER_CLASS(UPrimitiveComponent);
