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
#include "Serialization/Archive.h"

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

	// FStaticMesh에 이미 계산된 바운드가 있으면 그대로 사용
	if (!Asset->bBoundsValid)
	{
		Asset->CacheBounds();
	}

	CachedLocalCenter = Asset->BoundsCenter;
	CachedLocalExtent = Asset->BoundsExtent;
	bHasValidBounds = Asset->bBoundsValid;
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
	bHasValidWorldAABB = true;
}

bool UStaticMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();
	return LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, OutHitResult);
}

bool UStaticMeshComponent::LineTraceStaticMeshFast(
	const FRay& Ray,
	const FMatrix& WorldMatrix,
	const FMatrix& WorldInverse,
	FHitResult& OutHitResult)
{
	if (!StaticMesh) return false;

	FVector LocalOrigin = WorldInverse.TransformPositionWithW(Ray.Origin);
	FVector LocalDirection = WorldInverse.TransformVector(Ray.Direction);
	LocalDirection.Normalize();

	// Mesh BVH만 사용하는 전용 경로입니다. 월드 BVH는 이 함수를 직접 호출해 가상 호출 비용을 피합니다.
	if (StaticMesh->RaycastMeshTrianglesWithBVHLocal(LocalOrigin, LocalDirection, OutHitResult))
	{
		const FVector LocalHitPoint = LocalOrigin + LocalDirection * OutHitResult.Distance;
		const FVector WorldHitPoint = WorldMatrix.TransformPositionWithW(LocalHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	// 실패하면 기존 방식하던 걸 주석 처리. 성능개선이 일단 확인됨.
	//bool bHit = FRayUtils::RaycastTriangles(
	//	Ray, GetWorldMatrix(),
	//	GetWorldInverseMatrix(),
	//	&Asset->Vertices[0].pos,
	//	sizeof(FNormalVertex),
	//	Asset->Indices,
	//	OutHitResult);

	//if (bHit)
	//{
	//	OutHitResult.HitComponent = this;
	//}
	/*codex의 답변
왜 빨라졌냐면, 주석 처리된 Jungle_Week5_Team3/KraftonEngine/Source/Engine/Collision/RayUtils.cpp:60의
RaycastTriangles()는 BVH 없이 Indices를 처음부터 끝까지 3개씩 돌면서 모든 triangle에 IntersectTriangle()를 합니다.
즉 후보 메시에 대해 매번 풀 스캔입니다.
월드 단계에서 이미 Jungle_Week5_Team3/KraftonEngine/Source/Engine/Collision/WorldPrimitivePickingBVH.cpp:87가
primitive AABB 기준으로 후보만 추립니다.
그런데 AABB는 보수적이라 “박스는 맞았지만 실제 삼각형은 안 맞는” 후보가 꽤 나옵니다.
예전 코드에서는 이런 BVH miss 후보마다 fallback 전체 triangle 스캔이 한 번 더 돌았습니다.
즉 “안 맞은 객체”를 확인하는 비용이 너무 컸던 겁니다.
	*/
	return false; // bHit;
}

// FArchive 기반 직렬화 — 복제 왕복용. 자산은 경로로만 들고, 실제 로드는 PostDuplicate에서.
static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	Ar << Slot.bUVScroll;
	return Ar;
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << StaticMeshPath;
	Ar << MaterialSlots;
}

void UStaticMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	// 메시 에셋 재로딩
	if (!StaticMeshPath.empty() && StaticMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(StaticMeshPath, Device);
		if (Loaded)
		{
			// SetStaticMesh는 MaterialSlots를 덮어쓰므로, 직렬화된 슬롯 정보를 백업·복원한다.
			TArray<FMaterialSlot> SavedSlots = MaterialSlots;
			SetStaticMesh(Loaded);

			// Override material 재로딩
			for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
			{
				MaterialSlots[i] = SavedSlots[i];
				const FString& MatPath = MaterialSlots[i].Path;
				if (MatPath.empty() || MatPath == "None")
				{
					OverrideMaterials[i] = nullptr;
				}
				else
				{
					UMaterial* LoadedMat = FObjManager::GetOrLoadMaterial(MatPath);
					if (LoadedMat)
					{
						if (!LoadedMat->DiffuseTexture && !LoadedMat->DiffuseTextureFilePath.empty())
						{
							LoadedMat->DiffuseTexture = UTexture2D::LoadFromFile(LoadedMat->DiffuseTextureFilePath, Device);
						}
					}
					OverrideMaterials[i] = LoadedMat;
				}
			}
		}
	}

	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
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
	UPrimitiveComponent::PostEditProperty(PropertyName);

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
