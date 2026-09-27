#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Resource/ShaderManager.h"

#include <cstring>

namespace
{
	constexpr uint32 FNV1A_OFFSET_BASIS = 2166136261u;
	constexpr uint32 FNV1A_PRIME = 16777619u;

	uint32 HashCombine(uint32 Seed, uint32 Value)
	{
		return (Seed ^ Value) * FNV1A_PRIME;
	}

	uint32 HashPointer(const void* Ptr)
	{
		const uint64 Value = static_cast<uint64>(reinterpret_cast<uintptr_t>(Ptr) >> 4);
		return static_cast<uint32>(Value ^ (Value >> 32));
	}

	uint32 HashFloatBits(float Value)
	{
		uint32 Bits = 0;
		std::memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	uint32 HashSectionMaterialState(const FMeshSectionDraw& Section)
	{
		uint32 Hash = FNV1A_OFFSET_BASIS;
		Hash = HashCombine(Hash, HashPointer(Section.DiffuseSRV));
		Hash = HashCombine(Hash, Section.bIsUVScroll ? 1u : 0u);
		Hash = HashCombine(Hash, HashFloatBits(Section.DiffuseColor.X));
		Hash = HashCombine(Hash, HashFloatBits(Section.DiffuseColor.Y));
		Hash = HashCombine(Hash, HashFloatBits(Section.DiffuseColor.Z));
		Hash = HashCombine(Hash, HashFloatBits(Section.DiffuseColor.W));
		return Hash;
	}

	uint32 HashMaterialLayout(const TArray<FMeshSectionDraw>& Sections)
	{
		uint32 Hash = FNV1A_OFFSET_BASIS;
		Hash = HashCombine(Hash, static_cast<uint32>(Sections.size()));

		for (const FMeshSectionDraw& Section : Sections)
		{
			Hash = HashCombine(Hash, HashSectionMaterialState(Section));
		}

		return Hash;
	}
}

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
	: Owner(InComponent)
{
	bSupportsOutline = Owner->SupportsOutline();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = Owner->GetWorldBoundingBox();
	LastLODUpdateFrame = UINT32_MAX;
	MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
	// 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
	bVisible = Owner->IsVisible();
	if (bVisible)
	{
		AActor* OwnerActor = Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible())
			bVisible = false;
	}
}

void FPrimitiveSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Pass = ERenderPass::Opaque;
	UpdateSortKey();
}

void FPrimitiveSceneProxy::UpdateSortKey()
{
	SortKey = (static_cast<uint64>(reinterpret_cast<uintptr_t>(Shader) >> 4) << 32)
		| (static_cast<uint64>(reinterpret_cast<uintptr_t>(MeshBuffer) >> 4) & 0xFFFFFFFF);
	MaterialSortKey = HashMaterialLayout(SectionDraws);
}
