#include "UAxisComp.h"

#include "URender.h"
#include "ResourceManager.h"
UAxisComp::UAxisComp()
{
	ResourceManager* RM = ResourceManager::GetInstance();
	VResource = &RM->GetVertexResource("Axis");
}

UAxisComp::~UAxisComp()
{
}

URenderObject* UAxisComp::Render()
{
	UObject* renderObj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* RenderObj = dynamic_cast<URenderObject*>(renderObj);
	ResourceManager* RM = ResourceManager::GetInstance();

	RenderObj->SetObject(VResource->VertexBuffer,
		RM->GetVertexShader(L"ShaderW0.hlsl"),
		RM->GetPixelShader(L"ShaderW0.hlsl"),
		RM->GetInputLayout(L"ShaderW0.hlsl"),
		VResource->vertexCount,
		VResource->offset,
		VResource->stride);

	RenderObjectConstants Constants;
	Constants.Model = GetRelativeMatrix();
	RenderObj->SetConstants(Constants);

	return RenderObj;
}

void UAxisComp::Update(float DeltaTime)
{
	float Speed = 10.f;
	//	RelativeRotation.x += DeltaTime * Speed;
	RelativeRotation.y += DeltaTime * Speed;
	//	RelativeRotation.z += DeltaTime * Speed;
}

REGISTER_CLASS(UAxisComp);