#include "UCubeComp.h"
#include "URender.h"
#include "ResourceManager.h"
UCubeComp::UCubeComp()
{
}

UCubeComp::~UCubeComp()
{
}

URenderObject* UCubeComp::Render()
{
	UObject* renderObj = FObjectFactory::ConstructObject(URenderObject::GetClass());
	URenderObject* RenderObj = dynamic_cast<URenderObject*>(renderObj);
	ResourceManager* RM = ResourceManager::GetInstance();

	VertexResource Resource = RM->GetVertexResource("Cube");

	RenderObj->SetObject(Resource.VertexBuffer,
		RM->GetVertexShader(L"ShaderW0.hlsl"),
		RM->GetPixelShader(L"ShaderW0.hlsl"),
		RM->GetInputLayout(L"ShaderW0.hlsl"),
		Resource.vertexCount,
		Resource.offset,
		Resource.stride);

	RenderObjectConstants Constants;
	Constants.Model = GetRelativeMatrix();
	RenderObj->SetConstants(Constants);

	return RenderObj;
}

void UCubeComp::Update(float DeltaTime)
{
	RelativeRotation.x += DeltaTime;
	RelativeRotation.y += DeltaTime;
	RelativeRotation.z += DeltaTime;
}

REGISTER_CLASS(UCubeComp);
