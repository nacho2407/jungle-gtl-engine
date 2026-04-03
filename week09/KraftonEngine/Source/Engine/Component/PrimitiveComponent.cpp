#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Core/CollisionTypes.h"
#include "Render/Pipeline/FScene.h"
#include "Render/Pipeline/PrimitiveSceneProxy.h"
#include "GameFramework/World.h"

#include <cstring>

DEFINE_CLASS(UPrimitiveComponent, USceneComponent)

void UPrimitiveComponent::MarkProxyDirty(EDirtyFlag Flag) const
{
	if (!SceneProxy || !Owner || !Owner->GetWorld()) return;
	Owner->GetWorld()->GetScene().MarkProxyDirty(SceneProxy, Flag);
}

void UPrimitiveComponent::SetVisibility(bool bNewVisible)
{
	if (bIsVisible == bNewVisible) return;
	bIsVisible = bNewVisible;
	MarkProxyDirty(EDirtyFlag::Visibility);
}

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Visible") == 0)
	{
		// Property Editor가 bIsVisible을 직접 수정하므로, 프록시 dirty 전파
		MarkProxyDirty(EDirtyFlag::Visibility);
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
	return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
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
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMeshData* Data = GetMeshData();
	if (!Data || Data->Indices.empty()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, CachedWorldMatrix,
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
	UpdateWorldAABB();

	// 프록시가 등록된 경우 Transform dirty 전파 (FScene DirtySet에도 등록)
	MarkProxyDirty(EDirtyFlag::Transform);
}

// --- 프록시 팩토리 ---
FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
	// 기본 PrimitiveComponent용 프록시
	return new FPrimitiveSceneProxy(this);
}

// --- 렌더 상태 관리 (UE RegisterComponent 대응) ---
void UPrimitiveComponent::CreateRenderState()
{
	if (SceneProxy) return; // 이미 등록됨

	// Owner → World → FScene 경로로 접근
	if (!Owner || !Owner->GetWorld()) return;
	FScene& Scene = Owner->GetWorld()->GetScene();
	SceneProxy = Scene.AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
	if (!SceneProxy) return;

	if (Owner && Owner->GetWorld())
	{
		FScene& Scene = Owner->GetWorld()->GetScene();
		Scene.RemovePrimitive(SceneProxy);
	}
	SceneProxy = nullptr;
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	// 프록시 파괴 후 재생성 — 메시 교체 등 큰 변경 시 사용
	DestroyRenderState();
	CreateRenderState();
}
