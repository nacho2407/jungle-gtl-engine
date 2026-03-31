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

void UStaticMeshComp::CollectRender(FRenderBus& Bus) const
{
	if (!Bus.GetShowFlags().bPrimitives) return;
	FMeshBuffer* Buffer = GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;

	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(GetWorldMatrix());
	Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::StaticMesh);
	Cmd.MeshBuffer = Buffer;

	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
	{
		const auto& Sections = StaticMesh->GetStaticMeshAsset()->Sections;
		const auto& Slots = StaticMesh->GetStaticMaterials();

		for (const FStaticMeshSection& Section : Sections)
		{
			FMeshSectionDraw Draw;
			Draw.FirstIndex = Section.FirstIndex;
			Draw.IndexCount = Section.NumTriangles * 3;

			for (int32 i = 0; i < Slots.size(); ++i)
			{
				if (Slots[i].MaterialSlotName == Section.MaterialSlotName)
				{
					if (i < OverrideMaterials.size() && OverrideMaterials[i])
					{
						auto& Mat = OverrideMaterials[i];
						if (Mat->DiffuseTexture)
							Draw.DiffuseSRV = Mat->DiffuseTexture->GetSRV();
						Draw.DiffuseColor = Mat->DiffuseColor;
					}
					break;
				}
			}
			Cmd.SectionDraws.push_back(Draw);
		}
	}
	Bus.AddCommand(ERenderPass::Opaque, Cmd);
}

void UStaticMeshComp::CollectSelection(FRenderBus& Bus) const
{
	FMeshBuffer* Buffer = GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;

	// SelectionMask: 스텐실에 선택 오브젝트 마킹
	FRenderCommand MaskCmd = {};
	MaskCmd.MeshBuffer = Buffer;
	MaskCmd.PerObjectConstants = FPerObjectConstants{ GetWorldMatrix() };
	MaskCmd.Shader = FShaderManager::Get().GetShader(EShaderType::StaticMesh);
	Bus.AddCommand(ERenderPass::SelectionMask, MaskCmd);

	if (Bus.GetShowFlags().bBoundingVolume)
	{
		FAABBEntry Entry = {};
		FBoundingBox Box = GetWorldBoundingBox();
		Entry.AABB.Min = Box.Min;
		Entry.AABB.Max = Box.Max;
		Entry.AABB.Color = FColor::White();
		Bus.AddAABBEntry(std::move(Entry));
	}
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
		OutProps.push_back({ "Element " + std::to_string(i), EPropertyType::Material, &OverrideMaterialPaths[i] });
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

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		// "Element 0"에서 8번째 인덱스부터 시작하는 숫자를 정수로 변환
		int32 Index = atoi(&PropertyName[8]);

		// 인덱스 범위 유효성 검사
		if (Index >= 0 && Index < OverrideMaterialPaths.size())
		{
			FString NewMatPath = OverrideMaterialPaths[Index];

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				// UI 콤보박스는 이미 로드된 머티리얼 목록을 보여주므로, 
				// TObjectIterator를 통해 메모리에 있는 해당 경로의 머티리얼을 찾습니다.
				UMaterial* FoundMat = nullptr;
				for (TObjectIterator<UMaterial> It; It; ++It)
				{
					if ((*It)->GetAssetPathFileName() == NewMatPath)
					{
						FoundMat = *It;
						break;
					}
				}

				if (FoundMat)
				{
					// 찾은 머티리얼을 해당 슬롯에 적용합니다. 
					// shared_ptr 관리 방식에 따라 아래와 같이 캐스팅하여 넣습니다.
					SetMaterial(Index, std::shared_ptr<UMaterial>(FoundMat, [](UMaterial*) {
						// UObjectManager가 수명을 관리하므로 shared_ptr이 해제될 때 
						// 실제 객체를 delete하지 않도록 빈 삭제자(No-op deleter)를 사용합니다.
						}));
				}
			}
		}
	}
}
