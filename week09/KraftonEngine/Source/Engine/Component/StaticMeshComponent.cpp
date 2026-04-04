#include "Component/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Collision/RayUtils.h"
#include "Mesh/StaticMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Resource/ShaderManager.h"
#include "Texture/Texture2D.h"
#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

IMPLEMENT_CLASS(UStaticMeshComponent, UMeshComponent)

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	return new FStaticMeshSceneProxy(this);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InMesh)
{
	StaticMesh = InMesh;
	if (InMesh)
	{
		StaticMeshPath = InMesh->GetAssetPathFileName();
		const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh->GetStaticMaterials();

		OverrideMaterials.resize(DefaultMaterials.size());
		MaterialSlots.resize(DefaultMaterials.size());

		for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
		{
			OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;
			MaterialSlots[i].bUVScroll = DefaultMaterials[i].bIsUVScroll ? 1 : 0;

			if (OverrideMaterials[i])
				MaterialSlots[i].Path = OverrideMaterials[i]->GetAssetPathFileName();
			else
				MaterialSlots[i].Path = "None";
		}
	}
	else
	{
		StaticMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void UStaticMeshComponent::CacheLocalBounds()
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

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	// 인덱스가 배열 범위를 벗어나지 않는지 안전 검사 (IsValidIndex 등 사용)
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		OverrideMaterials[ElementIndex] = InMaterial;

		// 프록시에 Material dirty 전파
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

UMaterial* UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMeshBuffer* UStaticMeshComponent::GetMeshBuffer() const
{
	if (!StaticMesh) return nullptr;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

void UStaticMeshComponent::UpdateWorldAABB() const
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
	bWorldAABBDirty = false;
}

bool UStaticMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!StaticMesh) return false;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty() || Asset->Indices.empty()) return false;

	FVector LocalOrigin = GetWorldInverseMatrix().TransformPositionWithW(Ray.Origin);
	FVector LocalDirection = GetWorldInverseMatrix().TransformVector(Ray.Direction);
	LocalDirection.Normalize();

	if (StaticMesh->RaycastMeshBVHLocal(LocalOrigin, LocalDirection, OutHitResult))
	{
		const FVector LocalHitPoint = LocalOrigin + LocalDirection * OutHitResult.Distance;
		const FVector WorldHitPoint = GetWorldMatrix().TransformPositionWithW(LocalHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, GetWorldMatrix(),
		GetWorldInverseMatrix(),
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

void UStaticMeshComponent::Serialize(bool bIsLoading, json::JSON& Handle)
{
}

void UStaticMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Static Mesh", EPropertyType::StaticMeshRef, &StaticMeshPath });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
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
		MarkWorldBoundsDirty();
	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		// "Element 0"에서 8번째 인덱스부터 시작하는 숫자를 정수로 변환
		int32 Index = atoi(&PropertyName[8]);

		// 인덱스 범위 유효성 검사
		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index].Path;

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterial* LoadedMat = FObjManager::GetOrLoadMaterial(NewMatPath);

				if (LoadedMat)
				{
					if (!LoadedMat->DiffuseTexture && !LoadedMat->DiffuseTextureFilePath.empty())
					{
						// GEngine을 통해 전역 Device를 가져와서 텍스처를 생성합니다.
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						LoadedMat->DiffuseTexture = UTexture2D::LoadFromFile(LoadedMat->DiffuseTextureFilePath, Device);
					}

					// 로드되거나 찾아진 머티리얼을 슬롯에 적용
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}
}
