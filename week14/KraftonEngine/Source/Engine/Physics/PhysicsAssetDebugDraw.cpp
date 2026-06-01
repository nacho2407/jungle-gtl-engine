#include "PhysicsAssetDebugDraw.h"

#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "Math/Matrix.h"
#include "Core/Logging/Log.h"

#include <cmath>

namespace
{
	constexpr float PhysicsAssetDebugDrawDuration = 0.05f;

	// 본 로컬 점 → 월드.
	// TRS 분해(scale 프레임 손실) 대신 본 월드 행렬 전체로 직접 변환한다.
	// 표시 메시의 스키닝(Position * InverseBindPose * BoneGlobal)과 동일 공간이 되어
	// 바인드 포즈에 스케일이 끼어 있어도 도형 크기/위치가 정확히 맞는다.
	FVector BoneLocalToWorld(const FMatrix& BoneMatrix, const FVector& LocalPoint)
	{
		return BoneMatrix.TransformPositionWithW(LocalPoint);
	}

	// 행렬의 평균 스케일 — 구/캡슐 반지름(스칼라)에만 사용.
	float AverageScale(const FMatrix& M)
	{
		const FVector S = M.GetScale();
		return (std::fabs(S.X) + std::fabs(S.Y) + std::fabs(S.Z)) / 3.0f;
	}

	void DrawSphereElem(UWorld* World, const FMatrix& Bone, float Scale,
		const FVector& Center, float Radius, const FColor& Color)
	{
		DrawDebugSphere(World, BoneLocalToWorld(Bone, Center), Radius * Scale, 16, Color, PhysicsAssetDebugDrawDuration);
	}

	void DrawBoxElem(UWorld* World, const FMatrix& Bone,
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

	void DrawSphylElem(UWorld* World, const FMatrix& Bone, float Scale,
		const FVector& Center, const FQuat& ShapeRot, float Radius, float Length, const FColor& Color)
	{
		const float   HalfLen = Length * 0.5f;
		const FVector Axis  = ShapeRot.RotateVector(FVector::ZAxisVector); // 캡슐 기본축 +Z
		const FVector PerpA = ShapeRot.RotateVector(FVector::XAxisVector);
		const FVector PerpB = ShapeRot.RotateVector(FVector::YAxisVector);

		const FVector TopLocal = Center + Axis * HalfLen;
		const FVector BotLocal = Center - Axis * HalfLen;

		// 양 끝 반구를 구로 근사 (반지름은 행렬 평균 스케일 적용)
		DrawDebugSphere(World, BoneLocalToWorld(Bone, TopLocal), Radius * Scale, 12, Color, PhysicsAssetDebugDrawDuration);
		DrawDebugSphere(World, BoneLocalToWorld(Bone, BotLocal), Radius * Scale, 12, Color, PhysicsAssetDebugDrawDuration);

		// 옆면 4 라인 (끝점은 본 로컬에서 만든 뒤 행렬로 변환 → 스케일 자동 반영)
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
	FPhysicsAssetDebugDrawOptions Options;
	Options.BodyColor = Color;
	DrawPhysicsAssetDebug(World, Asset, MeshComp, Options);
}

void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset,
	const USkinnedMeshComponent* MeshComp, const FPhysicsAssetDebugDrawOptions& Options)
{
	if (!World || !Asset || !MeshComp)
	{
		return;
	}

	static bool bLoggedOnce = false;
	const bool  bLog = !bLoggedOnce;
	int32       LogCount = 0;

	if (Options.bDrawBodies)
	{
		for (const UBodySetup* Body : Asset->GetBodySetups())
		{
			if (!Body)
			{
				continue;
			}

			// 이 바디가 붙은 본의 월드 행렬 (스킨 행렬과 동일한 BoneGlobal * World)
			FMatrix BoneMatrix;
			if (!MeshComp->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), BoneMatrix))
			{
				continue; // 메시에 해당 본 없음
			}

			const float            Scale = AverageScale(BoneMatrix);
			const FKAggregateGeom& Geom  = Body->GetAggGeom();

			if (bLog && LogCount < 6)
			{
				const FVector MScale = BoneMatrix.GetScale();
				const FVector MLoc   = BoneMatrix.GetLocation();

				FVector StoredSize = FVector::ZeroVector; // 저장된 도형 크기(반치수/반지름)
				const char* Kind = "none";
				if (!Geom.BoxElems.empty())   { StoredSize = Geom.BoxElems[0].Extents; Kind = "box"; }
				else if (!Geom.SphylElems.empty()) { StoredSize = FVector(Geom.SphylElems[0].Radius, Geom.SphylElems[0].Length, 0.0f); Kind = "sphyl(R,L)"; }
				else if (!Geom.SphereElems.empty()) { StoredSize = FVector(Geom.SphereElems[0].Radius, 0.0f, 0.0f); Kind = "sphere(R)"; }

				UE_LOG("[PhysDraw] Bone=%s | [4]stored %s=(%.2f,%.2f,%.2f) | [5]BoneMatScale=(%.4f,%.4f,%.4f) avg=%.4f | loc=(%.2f,%.2f,%.2f) | [6]world~=stored*avg=%.2f",
					Body->GetBoneName().ToString().c_str(),
					Kind, StoredSize.X, StoredSize.Y, StoredSize.Z,
					MScale.X, MScale.Y, MScale.Z, Scale,
					MLoc.X, MLoc.Y, MLoc.Z,
					StoredSize.X * Scale);
				++LogCount;
			}

			for (const FKSphereElem& S : Geom.SphereElems)
			{
				DrawSphereElem(World, BoneMatrix, Scale, S.Center, S.Radius, Options.BodyColor);
			}
			for (const FKBoxElem& B : Geom.BoxElems)
			{
				DrawBoxElem(World, BoneMatrix, B.Center, B.Rotation, B.Extents, Options.BodyColor);
			}
			for (const FKSphylElem& C : Geom.SphylElems)
			{
				DrawSphylElem(World, BoneMatrix, Scale, C.Center, C.Rotation, C.Radius, C.Length, Options.BodyColor);
			}
		}
	}

	// ── Constraints (바디는 노랑, 조인트는 초록으로 구분) ──
	if (Options.bDrawConstraints)
	{
		for (const UPhysicsConstraintTemplate* Constraint : Asset->GetConstraintTemplates())
		{
			if (!Constraint)
			{
				continue;
			}

			FMatrix ParentMat, ChildMat;
			if (!MeshComp->GetBoneWorldMatrixByName(Constraint->GetParentBoneName().ToString(), ParentMat))
			{
				continue;
			}
			if (!MeshComp->GetBoneWorldMatrixByName(Constraint->GetChildBoneName().ToString(), ChildMat))
			{
				continue;
			}

			// 조인트 위치 = 부모 본 행렬 × FrameA의 위치 (FrameA는 부모 본 로컬 기준)
			const FVector JointWorld   = ParentMat.TransformPositionWithW(Constraint->GetLocalFrameA().Location);
			const FVector ParentOrigin = ParentMat.GetLocation();
			const FVector ChildOrigin  = ChildMat.GetLocation();

			// 부모 → 조인트 → 자식 연결선
			DrawDebugLine(World, ParentOrigin, JointWorld, Options.ConstraintColor, PhysicsAssetDebugDrawDuration);
			DrawDebugLine(World, JointWorld,  ChildOrigin, Options.ConstraintColor, PhysicsAssetDebugDrawDuration);

			// 조인트 위치 마커(3축 십자) — 크기는 부모-자식 거리에 비례
			const float CrossSize = (ChildOrigin - ParentOrigin).Length() * 0.15f;
			DrawDebugPoint(World, JointWorld, CrossSize, Options.ConstraintColor, PhysicsAssetDebugDrawDuration);
		}
	}
	
	if (bLog)
	{
		bLoggedOnce = true;
	}
}
