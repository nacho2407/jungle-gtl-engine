#pragma once
#include "Component/PrimitiveComponent.h"

#include "LineBatchComponent.generated.h"

struct FComponentLineVertex
{
	FVector Position;
	FColor Color;

	FComponentLineVertex(const FVector& InPos, const FColor& InColor) : Position(InPos), Color(InColor) {}
};

class UMaterialInterface;

// 사용하지 않는 컴포넌트입니다.
// RenderProxy가 도입되면 사용될 예정입니다.
UCLASS()
class ULineBatchComponent : public UPrimitiveComponent
{
    GENERATED_BODY_ULineBatchComponent()
public:
	ULineBatchComponent();
	virtual ~ULineBatchComponent() override = default;

	virtual EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Line; }

	void AddLine(const FVector& Start, const FVector& End, const FColor& Color);
	void AddAABB(const FAABB& Box, const FColor& Color);
	void AddOBB(const FOBB& Box, const FColor& Color);

	UMaterialInterface* GetMaterial() const { return Material; }

private:
	UMaterialInterface* Material = nullptr;

	TArray<FComponentLineVertex> LineVertices;
	TArray<uint32> Indices;
};
