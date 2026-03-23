#include "PrimitiveComponent.h"
#include "Renderer/Renderer.h"
#include "EngineCore/ResourceManager.h"
#include "Scene.h"
#include "AABB.h"
#include "LineBatcher.h"
UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	if (RenderObj != nullptr)
	{
		if (Scene != nullptr)
		{
			Scene->Level.UnregisterRenderObject(RenderObj);
		}
		delete RenderObj;
	}
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
	if (RenderObj != nullptr && Scene != nullptr)
		Scene->Level.RegisterRenderObject(RenderObj);
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
	RenderObj->ShowFlag = EEngineShowFlags::SF_Primitives;
	UpdateRenderObject();
	return RenderObj;
}

void UPrimitiveComponent::UpdateRenderObject()
{
	RenderObj->World = GetRelativeMatrix();
	RenderObj->bIsSelected = bIsSelected;
	RenderObj->ShowFlag = EEngineShowFlags::SF_Primitives;
}

RenderObject* UPrimitiveComponent::GetRenderObject()
{
	return RenderObj;
}

void UPrimitiveComponent::DrawDebugLines()
{
	FVector Scale = GetRelativeScale3D();
	FMatrix ScaleMatrix = FMatrix::ScaleMatrix(Scale.x, Scale.y, Scale.z);
	FVector Location = GetRelativeLocation();
	FMatrix LocationMatrix = FMatrix::TranslationMatrix(Location.x, Location.y, Location.z);
	BoundingBox.RelativeMatrix = ScaleMatrix * LocationMatrix;
	FLineBatcher::GetInstance().AddAABB(BoundingBox, FColor(1.f, 0.f, 0.f));
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

void UPrimitiveComponent::CreateAABB()
{
	if (VResource == nullptr) return;

	for (auto& vertex : VResource->Vertices)
	{
		BoundingBox.MinPos.x = vertex.x < BoundingBox.MinPos.x ? vertex.x : BoundingBox.MinPos.x;
		BoundingBox.MaxPos.x = vertex.x > BoundingBox.MaxPos.x ? vertex.x : BoundingBox.MaxPos.x;
		BoundingBox.MinPos.y = vertex.y < BoundingBox.MinPos.y ? vertex.y : BoundingBox.MinPos.y;
		BoundingBox.MaxPos.y = vertex.y > BoundingBox.MaxPos.y ? vertex.y : BoundingBox.MaxPos.y;
		BoundingBox.MinPos.z = vertex.z < BoundingBox.MinPos.z ? vertex.z : BoundingBox.MinPos.z;
		BoundingBox.MaxPos.z = vertex.z > BoundingBox.MaxPos.z ? vertex.z : BoundingBox.MaxPos.z;
	}
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
