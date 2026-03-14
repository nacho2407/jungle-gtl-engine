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

URenderObject* UAxisComp::Render()
{
	UObject* renderObj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* RenderObj = dynamic_cast<URenderObject*>(renderObj);
	ResourceManager* RM = ResourceManager::GetInstance();

	RenderObj->Geometry = &RM->GetGeometry("Axis");
	RenderObj->Material = &RM->GetMaterial(L"ShaderW0.hlsl");
	RenderObj->World = GetRelativeMatrix();

	return RenderObj;
}

void UAxisComp::Update(float DeltaTime)
{
	float Speed = 10.f;
	RelativeRotation.y += DeltaTime * Speed;
}

REGISTER_CLASS(UAxisComp);