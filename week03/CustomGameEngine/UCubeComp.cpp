#include "UCubeComp.h"
#include "URender.h"
#include "ResourceManager.h"
UCubeComp::UCubeComp()
{
	type = EPrimitiveType::Cube;
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetGeometry("Cube");
}

UCubeComp::~UCubeComp()
{
}

URenderObject* UCubeComp::Render()
{
	UObject* renderObj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* RenderObj = dynamic_cast<URenderObject*>(renderObj);
	ResourceManager* RM = ResourceManager::GetInstance();

	RenderObj->Geometry = &RM->GetGeometry("Cube");
	RenderObj->Material = &RM->GetMaterial(L"ShaderW0.hlsl");
	RenderObj->World = GetRelativeMatrix();
	RenderObj->bIsSelected = bIsSelected;

	return RenderObj;
}

void UCubeComp::Update(float DeltaTime)
{
	float Speed = 10.f;
	RelativeRotation.y += DeltaTime * Speed;
}

REGISTER_CLASS(UCubeComp);
