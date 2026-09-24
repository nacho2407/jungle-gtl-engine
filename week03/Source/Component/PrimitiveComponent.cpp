#include "PrimitiveComponent.h"

#include "Renderer/Renderer.h"
#include "EngineCore/ResourceManager.h"
#include "Scene.h"
#include "AABB.h"
#include "LineBatcher.h"
#include "Math/Ray.h"

UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	if (RenderObj != nullptr)
	{
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

void UPrimitiveComponent::SetRelativeQuaternion(FQuat newQuat)
{
	USceneComponent::SetRelativeQuaternion(newQuat);
	MarkRenderStateDirty();
}

void UPrimitiveComponent::OnComponentAdded()
{
	UpdateAABB();
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
	RenderObj->Material = &RM->GetMaterial(L"Shaders/ShaderW0.hlsl");
	RenderObj->ShowFlag = EEngineShowFlags::SF_Primitives;
	RenderObj->OwnerComponent = this;
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
	//FVector Scale = GetRelativeScale3D();
	//FMatrix ScaleMatrix = FMatrix::ScaleMatrix(Scale.x, Scale.y, Scale.z);
	//FVector Location = GetRelativeLocation();
	//FMatrix LocationMatrix = FMatrix::TranslationMatrix(Location.x, Location.y, Location.z);
	//BoundingBox.RelativeMatrix = ScaleMatrix * LocationMatrix;
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
	UpdateAABB();
	RenderObj->bIsDirty = false;
}

void UPrimitiveComponent::UpdateAABB()
{
	if (VResource == nullptr) return;

	BoundingBox.MinPos = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	BoundingBox.MaxPos = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	TArray<FVector> ChangedVertices;
	for (const auto& vertex : VResource->Vertices)
	{
		ChangedVertices.push_back(GetRelativeMatrix().TransformPoint(vertex));
	}
	for (const auto& vertex : ChangedVertices)
	{
		BoundingBox.MinPos.x = (std::min)(vertex.x, BoundingBox.MinPos.x);
		BoundingBox.MaxPos.x = (std::max)(vertex.x, BoundingBox.MaxPos.x);
		BoundingBox.MinPos.y = (std::min)(vertex.y, BoundingBox.MinPos.y);
		BoundingBox.MaxPos.y = (std::max)(vertex.y, BoundingBox.MaxPos.y);
		BoundingBox.MinPos.z = (std::min)(vertex.z, BoundingBox.MinPos.z);
		BoundingBox.MaxPos.z = (std::max)(vertex.z, BoundingBox.MaxPos.z);
	}
}

bool UPrimitiveComponent::RayIntersects(Ray& ray, float& OutDistance)
{
	//먼저 AABB 검사를 통과해야 메시와의 교차 검사를 수행합니다
	float AABBDistance = 0.0f;
	if (!BoundingBox.RayIntersects(ray, AABBDistance))
	{
		return false;
	}
	//정확한 교차 검사
	return ray.Intersects(this, OutDistance);
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
	case EPrimitiveType::TextMesh:
		jsonObj["Type"] = "TextMesh";
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
	else if (jsonObj["Type"].ToString() == "TextMesh")
		type = EPrimitiveType::TextMesh;
}



REGISTER_CLASS(UPrimitiveComponent);
