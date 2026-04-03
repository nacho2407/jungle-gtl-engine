#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Core/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include <cstring>

DEFINE_CLASS(UPrimitiveComponent, USceneComponent)

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Visible") == 0)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UWorld* World = OwnerActor->GetWorld())
			{
				World->MarkPickingBVHDirty();
			}
		}
	}
}

void UPrimitiveComponent::CollectRender(FRenderBus& Bus) const
{
	if (!Bus.GetShowFlags().bPrimitives) return;
	FMeshBuffer* Buffer = GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;

	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(GetWorldMatrix());
	Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Cmd.MeshBuffer = Buffer;
	Bus.AddCommand(ERenderPass::Opaque, Cmd);
}

void UPrimitiveComponent::CollectSelection(FRenderBus& Bus) const
{
	FMeshBuffer* Buffer = GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;
	if (!SupportsOutline()) return;

	// SelectionMask: 스텐실에 선택 오브젝트 마킹 (PostProcess에서 edge detection)
	FRenderCommand MaskCmd = {};
	MaskCmd.MeshBuffer = Buffer;
	MaskCmd.PerObjectConstants = FPerObjectConstants{ GetWorldMatrix() };
	MaskCmd.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
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

FBoundingBox UPrimitiveComponent::GetWorldBoundingBox() const
{
	EnsureWorldAABBUpdated();
	return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
}

void UPrimitiveComponent::SetVisibility(bool bVisible)
{
	if (bIsVisible == bVisible)
	{
		return;
	}

	bIsVisible = bVisible;
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			World->MarkPickingBVHDirty();
		}
	}
}

void UPrimitiveComponent::MarkWorldBoundsDirty()
{
	bWorldAABBDirty = true;
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			World->MarkPickingBVHDirty();
		}
	}
}

void UPrimitiveComponent::UpdateWorldAABB() const
{
	FVector LExt = LocalExtents;

	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
	bWorldAABBDirty = false;
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMeshData* Data = GetMeshData();
	if (!Data || Data->Indices.empty()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, GetWorldMatrix(),
		GetWorldInverseMatrix(),
		&Data->Vertices[0].Position,
		sizeof(FVertex),
		Data->Indices,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}
	return bHit;
}

void UPrimitiveComponent::UpdateWorldMatrix() const
{
	USceneComponent::UpdateWorldMatrix();
}

void UPrimitiveComponent::OnTransformDirty()
{
	MarkWorldBoundsDirty();
}

void UPrimitiveComponent::EnsureWorldAABBUpdated() const
{
	GetWorldMatrix();
	if (bWorldAABBDirty)
	{
		UpdateWorldAABB();
	}
}
