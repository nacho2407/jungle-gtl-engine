#include "CollisionDebugGeometry.h"

#include "Math/MathUtils.h"
#include "PhysicsEngine/BodySetup.h"

#include <cstddef>
#include <cmath>

namespace
{
	constexpr int32 WireSegments = 24;
	constexpr int32 SphereStacks = 12;
	constexpr int32 CapsuleHalfStacks = 6;

	bool IsValidConvexIndex(const FKConvexElem& ConvexElem, int32 Index)
	{
		return Index >= 0 && static_cast<size_t>(Index) < ConvexElem.VertexData.size();
	}

	void AddLine(TArray<FWireLine>& Lines, const FVector& Start, const FVector& End)
	{
		Lines.push_back({ Start, End });
	}

	void AddWireCircle(TArray<FWireLine>& Lines, const FVector& Center,
		const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	void AddWireHalfCircle(TArray<FWireLine>& Lines, const FVector& Center,
		const FVector& AxisA, const FVector& AxisB, float Radius, int32 Segments, float StartAngle)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + (AxisA * std::cos(StartAngle) + AxisB * std::sin(StartAngle)) * Radius;

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = StartAngle + Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	FVector TransformNormal(const FTransform& WorldTM, const FVector& Normal)
	{
		return WorldTM.TransformVectorNoScale(Normal).GetSafeNormal(1.0e-6f, FVector::UpVector);
	}

	uint32 AddSolidVertex(FPhysicsDebugSolidMesh& Mesh, const FTransform& WorldTM,
		const FVector& LocalPosition, const FVector& LocalNormal, const FVector4& Color)
	{
		const uint32 VertexIndex = static_cast<uint32>(Mesh.Vertices.size());
		Mesh.Vertices.push_back({
			WorldTM.TransformPosition(LocalPosition),
			TransformNormal(WorldTM, LocalNormal),
			Color
		});
		return VertexIndex;
	}

	void AddTriangle(FPhysicsDebugSolidMesh& Mesh,
		const FVector& P0, const FVector& N0,
		const FVector& P1, const FVector& N1,
		const FVector& P2, const FVector& N2,
		const FTransform& WorldTM, const FVector4& Color)
	{
		const uint32 I0 = AddSolidVertex(Mesh, WorldTM, P0, N0, Color);
		const uint32 I1 = AddSolidVertex(Mesh, WorldTM, P1, N1, Color);
		const uint32 I2 = AddSolidVertex(Mesh, WorldTM, P2, N2, Color);
		Mesh.Indices.push_back(I0);
		Mesh.Indices.push_back(I1);
		Mesh.Indices.push_back(I2);
	}

	void AddQuad(FPhysicsDebugSolidMesh& Mesh,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
		const FVector& Normal, const FTransform& WorldTM, const FVector4& Color)
	{
		AddTriangle(Mesh, P0, Normal, P1, Normal, P2, Normal, WorldTM, Color);
		AddTriangle(Mesh, P0, Normal, P2, Normal, P3, Normal, WorldTM, Color);
	}

	FVector SpherePoint(float Radius, float Theta, float Phi)
	{
		const float SinTheta = std::sin(Theta);
		return FVector(
			std::cos(Phi) * SinTheta * Radius,
			std::sin(Phi) * SinTheta * Radius,
			std::cos(Theta) * Radius);
	}

	FVector UnitSphereNormal(float Theta, float Phi)
	{
		return SpherePoint(1.0f, Theta, Phi).GetSafeNormal(1.0e-6f, FVector::UpVector);
	}

	void AddHemisphere(FPhysicsDebugSolidMesh& Mesh, const FTransform& WorldTM, float Radius,
		float CylinderHalf, bool bTop, const FVector4& Color)
	{
		for (int32 Stack = 0; Stack < CapsuleHalfStacks; ++Stack)
		{
			const float T0 = static_cast<float>(Stack) / static_cast<float>(CapsuleHalfStacks);
			const float T1 = static_cast<float>(Stack + 1) / static_cast<float>(CapsuleHalfStacks);
			const float Theta0 = T0 * FMath::Pi * 0.5f;
			const float Theta1 = T1 * FMath::Pi * 0.5f;

			for (int32 Segment = 0; Segment < WireSegments; ++Segment)
			{
				const float P0 = 2.0f * FMath::Pi * static_cast<float>(Segment) / static_cast<float>(WireSegments);
				const float P1 = 2.0f * FMath::Pi * static_cast<float>(Segment + 1) / static_cast<float>(WireSegments);

				FVector N00(std::cos(P0) * std::sin(Theta0), std::sin(P0) * std::sin(Theta0), std::cos(Theta0));
				FVector N10(std::cos(P1) * std::sin(Theta0), std::sin(P1) * std::sin(Theta0), std::cos(Theta0));
				FVector N11(std::cos(P1) * std::sin(Theta1), std::sin(P1) * std::sin(Theta1), std::cos(Theta1));
				FVector N01(std::cos(P0) * std::sin(Theta1), std::sin(P0) * std::sin(Theta1), std::cos(Theta1));

				if (!bTop)
				{
					N00.Z *= -1.0f;
					N10.Z *= -1.0f;
					N11.Z *= -1.0f;
					N01.Z *= -1.0f;
				}

				const float CenterZ = bTop ? CylinderHalf : -CylinderHalf;
				const FVector V00 = FVector(N00.X * Radius, N00.Y * Radius, CenterZ + N00.Z * Radius);
				const FVector V10 = FVector(N10.X * Radius, N10.Y * Radius, CenterZ + N10.Z * Radius);
				const FVector V11 = FVector(N11.X * Radius, N11.Y * Radius, CenterZ + N11.Z * Radius);
				const FVector V01 = FVector(N01.X * Radius, N01.Y * Radius, CenterZ + N01.Z * Radius);

				if (bTop)
				{
					AddTriangle(Mesh, V00, N00, V10, N10, V11, N11, WorldTM, Color);
					AddTriangle(Mesh, V00, N00, V11, N11, V01, N01, WorldTM, Color);
				}
				else
				{
					AddTriangle(Mesh, V00, N00, V11, N11, V10, N10, WorldTM, Color);
					AddTriangle(Mesh, V00, N00, V01, N01, V11, N11, WorldTM, Color);
				}
			}
		}
	}
}

namespace FCollisionDebugGeometry
{
	void AddWireSphere(TArray<FWireLine>& OutLines, const FVector& Center, float Radius)
	{
		AddWireCircle(OutLines, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), Radius, WireSegments);
		AddWireCircle(OutLines, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, WireSegments);
		AddWireCircle(OutLines, Center, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, WireSegments);
	}

	void AddWireBox(TArray<FWireLine>& OutLines, const FTransform& WorldTM, const FVector& HalfExtent)
	{
		FVector Corners[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector LocalOffset(
				(Index & 1) ? HalfExtent.X : -HalfExtent.X,
				(Index & 2) ? HalfExtent.Y : -HalfExtent.Y,
				(Index & 4) ? HalfExtent.Z : -HalfExtent.Z);
			Corners[Index] = WorldTM.TransformPosition(LocalOffset);
		}

		AddLine(OutLines, Corners[0], Corners[1]);
		AddLine(OutLines, Corners[1], Corners[3]);
		AddLine(OutLines, Corners[3], Corners[2]);
		AddLine(OutLines, Corners[2], Corners[0]);
		AddLine(OutLines, Corners[4], Corners[5]);
		AddLine(OutLines, Corners[5], Corners[7]);
		AddLine(OutLines, Corners[7], Corners[6]);
		AddLine(OutLines, Corners[6], Corners[4]);
		AddLine(OutLines, Corners[0], Corners[4]);
		AddLine(OutLines, Corners[1], Corners[5]);
		AddLine(OutLines, Corners[2], Corners[6]);
		AddLine(OutLines, Corners[3], Corners[7]);
	}

	void AddWireCapsule(TArray<FWireLine>& OutLines, const FTransform& WorldTM, float Radius, float Length)
	{
		const float CylinderHalf = FMath::Max(0.0f, Length * 0.5f);
		constexpr int32 HalfSegments = WireSegments / 2;

		const FVector AxisX = WorldTM.TransformVectorNoScale(FVector(1.0f, 0.0f, 0.0f)).GetSafeNormal();
		const FVector AxisY = WorldTM.TransformVectorNoScale(FVector(0.0f, 1.0f, 0.0f)).GetSafeNormal();
		const FVector AxisZ = WorldTM.TransformVectorNoScale(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();

		const FVector TopCenter = WorldTM.TransformPosition(FVector(0.0f, 0.0f, CylinderHalf));
		const FVector BottomCenter = WorldTM.TransformPosition(FVector(0.0f, 0.0f, -CylinderHalf));

		AddWireCircle(OutLines, TopCenter, AxisX, AxisY, Radius, WireSegments);
		AddWireCircle(OutLines, BottomCenter, AxisX, AxisY, Radius, WireSegments);

		AddLine(OutLines, TopCenter + AxisX * Radius, BottomCenter + AxisX * Radius);
		AddLine(OutLines, TopCenter - AxisX * Radius, BottomCenter - AxisX * Radius);
		AddLine(OutLines, TopCenter + AxisY * Radius, BottomCenter + AxisY * Radius);
		AddLine(OutLines, TopCenter - AxisY * Radius, BottomCenter - AxisY * Radius);

		AddWireHalfCircle(OutLines, TopCenter, AxisX, AxisZ, Radius, HalfSegments, 0.0f);
		AddWireHalfCircle(OutLines, TopCenter, AxisY, AxisZ, Radius, HalfSegments, 0.0f);
		AddWireHalfCircle(OutLines, BottomCenter, AxisX, AxisZ, Radius, HalfSegments, FMath::Pi);
		AddWireHalfCircle(OutLines, BottomCenter, AxisY, AxisZ, Radius, HalfSegments, FMath::Pi);
	}

	void AddWireConvex(TArray<FWireLine>& OutLines, const FKConvexElem& ConvexElem, const FTransform& WorldTM)
	{
		for (size_t Index = 0; Index + 2 < ConvexElem.IndexData.size(); Index += 3)
		{
			const int32 I0 = ConvexElem.IndexData[Index];
			const int32 I1 = ConvexElem.IndexData[Index + 1];
			const int32 I2 = ConvexElem.IndexData[Index + 2];
			if (!IsValidConvexIndex(ConvexElem, I0) || !IsValidConvexIndex(ConvexElem, I1) || !IsValidConvexIndex(ConvexElem, I2))
			{
				continue;
			}

			const FVector V0 = WorldTM.TransformPosition(ConvexElem.VertexData[I0]);
			const FVector V1 = WorldTM.TransformPosition(ConvexElem.VertexData[I1]);
			const FVector V2 = WorldTM.TransformPosition(ConvexElem.VertexData[I2]);
			AddLine(OutLines, V0, V1);
			AddLine(OutLines, V1, V2);
			AddLine(OutLines, V2, V0);
		}
	}

	void AddSolidSphere(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, float Radius, const FVector4& Color)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		for (int32 Stack = 0; Stack < SphereStacks; ++Stack)
		{
			const float Theta0 = FMath::Pi * static_cast<float>(Stack) / static_cast<float>(SphereStacks);
			const float Theta1 = FMath::Pi * static_cast<float>(Stack + 1) / static_cast<float>(SphereStacks);

			for (int32 Segment = 0; Segment < WireSegments; ++Segment)
			{
				const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Segment) / static_cast<float>(WireSegments);
				const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Segment + 1) / static_cast<float>(WireSegments);

				const FVector P00 = SpherePoint(Radius, Theta0, Phi0);
				const FVector P10 = SpherePoint(Radius, Theta0, Phi1);
				const FVector P11 = SpherePoint(Radius, Theta1, Phi1);
				const FVector P01 = SpherePoint(Radius, Theta1, Phi0);

				const FVector N00 = UnitSphereNormal(Theta0, Phi0);
				const FVector N10 = UnitSphereNormal(Theta0, Phi1);
				const FVector N11 = UnitSphereNormal(Theta1, Phi1);
				const FVector N01 = UnitSphereNormal(Theta1, Phi0);

				AddTriangle(OutMesh, P00, N00, P10, N10, P11, N11, WorldTM, Color);
				AddTriangle(OutMesh, P00, N00, P11, N11, P01, N01, WorldTM, Color);
			}
		}
	}

	void AddSolidBox(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, const FVector& HalfExtent, const FVector4& Color)
	{
		const float X = HalfExtent.X;
		const float Y = HalfExtent.Y;
		const float Z = HalfExtent.Z;

		AddQuad(OutMesh, FVector(X, -Y, -Z), FVector(X, Y, -Z), FVector(X, Y, Z), FVector(X, -Y, Z), FVector(1.0f, 0.0f, 0.0f), WorldTM, Color);
		AddQuad(OutMesh, FVector(-X, Y, -Z), FVector(-X, -Y, -Z), FVector(-X, -Y, Z), FVector(-X, Y, Z), FVector(-1.0f, 0.0f, 0.0f), WorldTM, Color);
		AddQuad(OutMesh, FVector(-X, Y, -Z), FVector(X, Y, -Z), FVector(X, Y, Z), FVector(-X, Y, Z), FVector(0.0f, 1.0f, 0.0f), WorldTM, Color);
		AddQuad(OutMesh, FVector(X, -Y, -Z), FVector(-X, -Y, -Z), FVector(-X, -Y, Z), FVector(X, -Y, Z), FVector(0.0f, -1.0f, 0.0f), WorldTM, Color);
		AddQuad(OutMesh, FVector(-X, -Y, Z), FVector(X, -Y, Z), FVector(X, Y, Z), FVector(-X, Y, Z), FVector(0.0f, 0.0f, 1.0f), WorldTM, Color);
		AddQuad(OutMesh, FVector(-X, Y, -Z), FVector(X, Y, -Z), FVector(X, -Y, -Z), FVector(-X, -Y, -Z), FVector(0.0f, 0.0f, -1.0f), WorldTM, Color);
	}

	void AddSolidCapsule(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, float Radius, float Length, const FVector4& Color)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		const float CylinderHalf = FMath::Max(0.0f, Length * 0.5f);

		for (int32 Segment = 0; Segment < WireSegments; ++Segment)
		{
			const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Segment) / static_cast<float>(WireSegments);
			const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Segment + 1) / static_cast<float>(WireSegments);

			const FVector N0(std::cos(Phi0), std::sin(Phi0), 0.0f);
			const FVector N1(std::cos(Phi1), std::sin(Phi1), 0.0f);
			const FVector P0(N0.X * Radius, N0.Y * Radius, -CylinderHalf);
			const FVector P1(N1.X * Radius, N1.Y * Radius, -CylinderHalf);
			const FVector P2(N1.X * Radius, N1.Y * Radius, CylinderHalf);
			const FVector P3(N0.X * Radius, N0.Y * Radius, CylinderHalf);
			AddTriangle(OutMesh, P0, N0, P1, N1, P2, N1, WorldTM, Color);
			AddTriangle(OutMesh, P0, N0, P2, N1, P3, N0, WorldTM, Color);
		}

		AddHemisphere(OutMesh, WorldTM, Radius, CylinderHalf, true, Color);
		AddHemisphere(OutMesh, WorldTM, Radius, CylinderHalf, false, Color);
	}

	void AddSolidConvex(FPhysicsDebugSolidMesh& OutMesh, const FKConvexElem& ConvexElem, const FTransform& WorldTM, const FVector4& Color)
	{
		for (size_t Index = 0; Index + 2 < ConvexElem.IndexData.size(); Index += 3)
		{
			const int32 I0 = ConvexElem.IndexData[Index];
			const int32 I1 = ConvexElem.IndexData[Index + 1];
			const int32 I2 = ConvexElem.IndexData[Index + 2];
			if (!IsValidConvexIndex(ConvexElem, I0) || !IsValidConvexIndex(ConvexElem, I1) || !IsValidConvexIndex(ConvexElem, I2))
			{
				continue;
			}

			const FVector Local0 = ConvexElem.VertexData[I0];
			const FVector Local1 = ConvexElem.VertexData[I1];
			const FVector Local2 = ConvexElem.VertexData[I2];
			const FVector World0 = WorldTM.TransformPosition(Local0);
			const FVector World1 = WorldTM.TransformPosition(Local1);
			const FVector World2 = WorldTM.TransformPosition(Local2);
			const FVector WorldNormal = (World1 - World0).Cross(World2 - World0).GetSafeNormal(1.0e-6f, FVector::UpVector);

			const uint32 BaseIndex = static_cast<uint32>(OutMesh.Vertices.size());
			OutMesh.Vertices.push_back({ World0, WorldNormal, Color });
			OutMesh.Vertices.push_back({ World1, WorldNormal, Color });
			OutMesh.Vertices.push_back({ World2, WorldNormal, Color });
			OutMesh.Indices.push_back(BaseIndex);
			OutMesh.Indices.push_back(BaseIndex + 1);
			OutMesh.Indices.push_back(BaseIndex + 2);
		}
	}
}

namespace FPhysicsBodyDebugGeometry
{
	namespace
	{
		void AppendPhysicsLines(TArray<FPhysicsDebugLine>& OutLines, const TArray<FWireLine>& Lines, const FVector4& Color)
		{
			for (const FWireLine& Line : Lines)
			{
				OutLines.push_back({ Line.Start, Line.End, Color });
			}
		}
	}

	void AddBodySetupWireLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const UBodySetup* BodySetup,
		FTransform BodyWorldTM,
		const FVector& Scale3D,
		bool bUseUniformScale,
		const FVector4& Color)
	{
		if (!BodySetup || BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			return;
		}

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		if (AggGeom.GetElementCount() == 0)
		{
			return;
		}

		TArray<FWireLine> Lines;
		BodyWorldTM.Scale = FVector::OneVector;

		if (bUseUniformScale)
		{
			const float UniformScale = Scale3D.GetAbsMax();

			for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
			{
				const FVector WorldCenter = BodyWorldTM.TransformPosition(SphereElem.Center * UniformScale);
				FCollisionDebugGeometry::AddWireSphere(Lines, WorldCenter, SphereElem.Radius * UniformScale);
			}

			for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
			{
				const FTransform ShapeWorldTM = FTransform(BoxElem.Center * UniformScale, BoxElem.Rotation) * BodyWorldTM;
				const FVector HalfExtent(
					BoxElem.X * 0.5f * UniformScale,
					BoxElem.Y * 0.5f * UniformScale,
					BoxElem.Z * 0.5f * UniformScale);
				FCollisionDebugGeometry::AddWireBox(Lines, ShapeWorldTM, HalfExtent);
			}

			for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
			{
				const FTransform ShapeWorldTM = FTransform(SphylElem.Center * UniformScale, SphylElem.Rotation) * BodyWorldTM;
				FCollisionDebugGeometry::AddWireCapsule(
					Lines,
					ShapeWorldTM,
					SphylElem.Radius * UniformScale,
					SphylElem.Length * UniformScale);
			}

			for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
			{
				FTransform ShapeWorldTM = ConvexElem.GetTransform() * BodyWorldTM;
				ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
				FCollisionDebugGeometry::AddWireConvex(Lines, ConvexElem, ShapeWorldTM);
			}
		}
		else
		{
			for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
			{
				const FKSphereElem ScaledSphere = SphereElem.GetFinalScaled(Scale3D, FTransform());
				const FTransform ShapeWorldTM = ScaledSphere.GetTransform() * BodyWorldTM;
				FCollisionDebugGeometry::AddWireSphere(Lines, ShapeWorldTM.GetLocation(), ScaledSphere.Radius);
			}

			for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
			{
				const FKBoxElem ScaledBox = BoxElem.GetFinalScaled(Scale3D, FTransform());
				const FTransform ShapeWorldTM = ScaledBox.GetTransform() * BodyWorldTM;
				const FVector HalfExtent(
					ScaledBox.X * 0.5f,
					ScaledBox.Y * 0.5f,
					ScaledBox.Z * 0.5f);
				FCollisionDebugGeometry::AddWireBox(Lines, ShapeWorldTM, HalfExtent);
			}

			for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
			{
				const FKSphylElem ScaledSphyl = SphylElem.GetFinalScaled(Scale3D, FTransform());
				const FTransform ShapeWorldTM = ScaledSphyl.GetTransform() * BodyWorldTM;
				FCollisionDebugGeometry::AddWireCapsule(
					Lines,
					ShapeWorldTM,
					ScaledSphyl.Radius,
					ScaledSphyl.Length);
			}

			for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
			{
				FTransform ShapeWorldTM = ConvexElem.GetTransform() * BodyWorldTM;
				ShapeWorldTM.Scale = ShapeWorldTM.Scale * Scale3D;
				FCollisionDebugGeometry::AddWireConvex(Lines, ConvexElem, ShapeWorldTM);
			}
		}

		AppendPhysicsLines(OutLines, Lines, Color);
	}
}
