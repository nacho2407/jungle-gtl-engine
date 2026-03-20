#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Common/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"

struct FMeshData;


class UPrimitiveComponent : public USceneComponent
{
private:
	FVector WorldAABBMinLocation;
	FVector WorldAABBMaxLocation;

protected:
	const FMeshData* MeshData = nullptr;
	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	bool bIsVisible = true;

public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
	inline const FMeshData* GetMeshData() const { return MeshData; };

	inline void SetVisibility(bool bVisible) { bIsVisible = bVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const
	{
		return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
	}

	//Collision
	void UpdateWorldAABB();
	bool CheckAABB(const FRay& Ray);
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1, const FVector& V2, float& OutT);
	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);
	inline bool IsVisible() const { return bIsVisible; }

	void UpdateWorldMatrix() override;

	virtual bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
		OutCommand.Type = ERenderCommandType::Primitive;
		OutCommand.TransformConstants.Model = GetWorldMatrix();
		OutCommand.TransformConstants.View = viewMatrix;
		OutCommand.TransformConstants.Projection = projMatrix;

		return true;
	}

	//	�� Primitive Component�� �ڽ��� � Primitive Type���� Renderer���� �˷��� �� �־�� �մϴ�. (Dynamic Binding)
	virtual EPrimitiveType GetPrimitiveType() const = 0;
};

class UCubeComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)
	UCubeComponent();
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Cube;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class USphereComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
	USphereComponent();
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Sphere;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class UPlaneComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
	UPlaneComponent();
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Plane;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

#if TEST

//class UStandfordBunnyComponent : public UPrimitiveComponent
//{
//private:
//
//public:
//	DECLARE_CLASS(UStandfordBunnyComponent, UPrimitiveComponent)
//	UStandfordBunnyComponent();
//	bool Raycast(const FRay& Ray, float& OutDistance) override;
//	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
//	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_StandfordBunny;
//
//	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
//};

#endif