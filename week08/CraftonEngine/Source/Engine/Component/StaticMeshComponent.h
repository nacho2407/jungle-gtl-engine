#pragma once

#include "Component/MeshComponent.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

namespace json { class JSON; }

// UStaticMeshComp — 월드 배치 컴포넌트
class UStaticMeshComp : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComp, UMeshComponent)

	UStaticMeshComp() = default;
	~UStaticMeshComp() override = default;

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_StaticMesh; }
	FMeshBuffer* GetMeshBuffer() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	void UpdateWorldAABB() const override;

	void SetStaticMesh(UStaticMesh* InMesh);
	UStaticMesh* GetStaticMesh() const;

	void Serialize(bool bIsLoading, json::JSON& Handle);

	// Property Editor 지원
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
	void CacheLocalBounds();

	UStaticMesh* StaticMesh = nullptr;
	FString StaticMeshPath = "None";
	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
