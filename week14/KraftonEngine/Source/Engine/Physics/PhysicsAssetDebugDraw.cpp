#include "PhysicsAssetDebugDraw.h"

#include "Physics/PhysicsAsset.h"
#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "Math/Transform.h"

namespace
{
	constexpr float PhysicsAssetDebugDrawDuration = 0.05f;

	// 본 로컬 점 → 월드 (스케일·회전, 이동 적용)
	FVector BoneLocalToWorld(const FTransform& Bone, const FVector& LocalPoint)
	{
		const FVector Scaled(
			LocalPoint.X * Bone.Scale.X,
			LocalPoint.Y * Bone.Scale.Y,
			LocalPoint.Z * Bone.Scale.Z);
		return Bone.Location + Bone.Rotation.RotateVector(Scaled);
	}

	void DrawSphereElem(UWorld* World, const FTransform& Bone,
		const FVector& Center, float Radius, const FColor& Color)
	{
		DrawDebugSphere(World, BoneLocalToWorld(Bone, Center), Radius, 16, Color, PhysicsAssetDebugDrawDuration);
	}

	void DrawBoxElem(UWorld* World, const FTransform& Bone,
		const FVector& Center, const FQuat& ShapeRot, const FVector& Extents, const FColor& Color)
	{
		// 박스 8 꼭짓점(도형 로컬, Extents=반치수)
		const FVector Signs[8] = {
			FVector(-1.0f,-1.0f,-1.0f), FVector( 1.0f,-1.0f,-1.0f),
			FVector( 1.0f, 1.0f,-1.0f), FVector(-1.0f, 1.0f,-1.0f),
			FVector(-1.0f,-1.0f, 1.0f), FVector( 1.0f,-1.0f, 1.0f),
			FVector( 1.0f, 1.0f, 1.0f), FVector(-1.0f, 1.0f, 1.0f)
		};

		FVector W[8];
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Corner(Signs[i].X * Extents.X, Signs[i].Y * Extents.Y, Signs[i].Z * Extents.Z);
			const FVector BoneLocal = Center + ShapeRot.RotateVector(Corner);
			W[i] = BoneLocalToWorld(Bone, BoneLocal);
		}

		DrawDebugBox(World, W[0], W[1], W[2], W[3], W[4], W[5], W[6], W[7], Color, PhysicsAssetDebugDrawDuration);
	}

	void DrawSphylElem(UWorld* World, const FTransform& Bone,
		const FVector& Center, const FQuat& ShapeRot, float Radius, float Length, const FColor& Color)
	{
		const float   HalfLen = Length * 0.5f;
		const FVector Axis  = ShapeRot.RotateVector(FVector::ZAxisVector); // 캡슐 기본축 +Z
		const FVector PerpA = ShapeRot.RotateVector(FVector::XAxisVector);
		const FVector PerpB = ShapeRot.RotateVector(FVector::YAxisVector);

		const FVector TopLocal = Center + Axis * HalfLen;
		const FVector BotLocal = Center - Axis * HalfLen;

		// 양 끝 반구를 구로 근사
		DrawDebugSphere(World, BoneLocalToWorld(Bone, TopLocal), Radius, 12, Color, PhysicsAssetDebugDrawDuration);
		DrawDebugSphere(World, BoneLocalToWorld(Bone, BotLocal), Radius, 12, Color, PhysicsAssetDebugDrawDuration);

		// 옆면 4 라인
		const FVector Dirs[4] = { PerpA, PerpA * -1.0f, PerpB, PerpB * -1.0f };
		for (const FVector& D : Dirs)
		{
			const FVector Top = BoneLocalToWorld(Bone, TopLocal + D * Radius);
			const FVector Bot = BoneLocalToWorld(Bone, BotLocal + D * Radius);
			DrawDebugLine(World, Top, Bot, Color, PhysicsAssetDebugDrawDuration);
		}
	}
}

void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset,
	const USkinnedMeshComponent* MeshComp, const FColor& Color)
{
	if (!World || !Asset || !MeshComp)
	{
		return;
	}

	for (const UBodySetup* Body : Asset->GetBodySetups())
	{
		if (!Body)
		{
			continue;
		}

		// 이 바디가 붙은 본의 현재 월드 트랜스폼
		FTransform BoneT;
		if (!MeshComp->GetBoneWorldTransformByName(Body->GetBoneName().ToString(), BoneT))
		{
			continue; // 메시에 해당 본 없음
		}

		const FKAggregateGeom& Geom = Body->GetAggGeom();

		for (const FKSphereElem& S : Geom.SphereElems)
		{
			DrawSphereElem(World, BoneT, S.Center, S.Radius, Color);
		}
		for (const FKBoxElem& B : Geom.BoxElems)
		{
			DrawBoxElem(World, BoneT, B.Center, B.Rotation, B.Extents, Color);
		}
		for (const FKSphylElem& C : Geom.SphylElems)
		{
			DrawSphylElem(World, BoneT, C.Center, C.Rotation, C.Radius, C.Length, Color);
		}
	}
}
