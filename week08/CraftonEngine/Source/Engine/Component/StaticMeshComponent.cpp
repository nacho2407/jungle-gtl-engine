#include "Component/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Collision/RayUtils.h"
#include "Mesh/StaticMeshAsset.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(UStaticMeshComp, UMeshComponent)

void UStaticMeshComp::SetStaticMesh(UStaticMesh* InMesh)
{
	StaticMesh = InMesh;
	if (InMesh)
	{
		StaticMeshPath = InMesh->GetAssetPathFileName();
		const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh->GetStaticMaterials();

		OverrideMaterials.resize(DefaultMaterials.size());
		OverrideMaterialPaths.resize(DefaultMaterials.size()); // 경로 배열도 사이즈 맞춤

		for (int32 i = 0; i < DefaultMaterials.size(); ++i)
		{
			OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

			if (OverrideMaterials[i])
				OverrideMaterialPaths[i] = OverrideMaterials[i]->GetAssetPathFileName();
			else
				OverrideMaterialPaths[i] = "None";
		}
	}
	else
	{
		StaticMeshPath = "None";
		OverrideMaterials.clear();
		OverrideMaterialPaths.clear();
	}
	CacheLocalBounds();
}

void UStaticMeshComp::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!StaticMesh) return;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return;

	FVector LocalMin = Asset->Vertices[0].pos;
	FVector LocalMax = Asset->Vertices[0].pos;
	for (const FNormalVertex& V : Asset->Vertices)
	{
		LocalMin.X = std::min(LocalMin.X, V.pos.X);
		LocalMin.Y = std::min(LocalMin.Y, V.pos.Y);
		LocalMin.Z = std::min(LocalMin.Z, V.pos.Z);
		LocalMax.X = std::max(LocalMax.X, V.pos.X);
		LocalMax.Y = std::max(LocalMax.Y, V.pos.Y);
		LocalMax.Z = std::max(LocalMax.Z, V.pos.Z);
	}

	CachedLocalCenter = (LocalMin + LocalMax) * 0.5f;
	CachedLocalExtent = (LocalMax - LocalMin) * 0.5f;
	bHasValidBounds = true;
}

UStaticMesh* UStaticMeshComp::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComp::SetMaterial(int32 ElementIndex, std::shared_ptr<UMaterial> InMaterial)
{
	// 인덱스가 배열 범위를 벗어나지 않는지 안전 검사 (IsValidIndex 등 사용)
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		OverrideMaterials[ElementIndex] = InMaterial;
	}
}

std::shared_ptr<UMaterial> UStaticMeshComp::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMeshBuffer* UStaticMeshComp::GetMeshBuffer() const
{
	if (!StaticMesh) return nullptr;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

void UStaticMeshComp::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
}

bool UStaticMeshComp::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!StaticMesh) return false;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty() || Asset->Indices.empty()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, CachedWorldMatrix,
		&Asset->Vertices[0].pos,
		sizeof(FNormalVertex),
		Asset->Indices,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}
	return bHit;
}

void UStaticMeshComp::Serialize(bool bIsLoading, json::JSON& Handle)
{
	/*if (bIsLoading)
	{
		FString AssetName; 
		
	}*/
}

void UStaticMeshComp::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Static Mesh", EPropertyType::StaticMeshRef, &StaticMeshPath });

	for (int32 i = 0; i < OverrideMaterialPaths.size(); ++i)
	{
		char Buffer[32];
		snprintf(Buffer, sizeof(Buffer), "Element %d", i);
		OutProps.push_back({ Buffer, EPropertyType::Material, &OverrideMaterialPaths[i] });
	}
}

void UStaticMeshComp::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Static Mesh") == 0)
	{
		if (StaticMeshPath.empty() || StaticMeshPath == "None")
		{
			StaticMesh = nullptr;
		}
		else
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(StaticMeshPath, Device);
			SetStaticMesh(Loaded);
		}
		CacheLocalBounds();
	}

	//if (strncmp(PropertyName, "Element", 8) == 0)
	//{
	//	// "Material X" 에서 인덱스 번호 추출
	//	int32 Index = atoi(&PropertyName[9]);

	//	if (Index >= 0 && Index < OverrideMaterialPaths.size())
	//	{
	//		FString NewMatPath = OverrideMaterialPaths[Index];
	//		if (NewMatPath != "None" && !NewMatPath.empty())
	//		{
	//			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	//			// ObjManager나 Factory를 통해 새 머티리얼 로드
	//			std::shared_ptr<UMaterial> LoadedMat = FObjManager::LoadObjMaterial(NewMatPath, Device);
	//			OverrideMaterials[Index] = LoadedMat; // 실제 스마트 포인터 교체
	//		}
	//		else
	//		{
	//			OverrideMaterials[Index] = nullptr;
	//		}
	//	}
	//}
}
