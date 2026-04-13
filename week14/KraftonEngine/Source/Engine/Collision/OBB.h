#pragma once
#include "Core/EngineTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

struct FOBB
{
	FVector Center = { 0, 0, 0 };
	FVector Extent { 0.5f, 0.5f, 0.5f };
	FRotator Rotation = { 0, 0, 0 };
	
	void ApplyTransform(FMatrix Matrix)
	{
		Center = Center * Matrix;
		
		FVector X = Rotation.GetForwardVector() * Extent.X;
		FVector Y = Rotation.GetRightVector() * Extent.Y;
		FVector Z = Rotation.GetUpVector() * Extent.Z;

		Matrix.M[3][0] = 0; Matrix.M[3][1] = 0; Matrix.M[3][2] = 0;

		X = X * Matrix;
		Y = Y * Matrix;
		Z = Z * Matrix;

		Extent.X = X.Length();
		Extent.Y = Y.Length();
		Extent.Z = Z.Length();

		FMatrix RotMat;
		RotMat.SetAxes(X.Normalized(), Y.Normalized(), Z.Normalized());
		Rotation = RotMat.ToRotator();
	}

	void UpdateAsOBB(const FMatrix& Matrix)
	{
		Center = Matrix.GetLocation();
		Rotation = Matrix.ToRotator();
		
		FVector Scale = Matrix.GetScale();
		Extent = Scale * 0.5f;
	}
	
	// SIMD 사용한 함수로 대체되었으나 SAT 공부를 위해 남겨둡니다. 현재 호출되는 위치 없음.
	bool IntersectOBBAABB(const FBoundingBox& AABB) const
	{
		// aabb를 obb의 local 공간으로 투영하는 행렬 계산
		float R[3][3], absR[3][3];
		for (int i = 0; i < 3; i++)
		{
			// AABB의 기본축(1,0,0...)과 OBB 축의 내적 = OBB 축의 성분값
			R[i][0] = Rotation.GetForwardVector().Data[i]; 
			absR[i][0] = std::abs(R[i][0]) + 1e-6f; // 수치적 안정성을 위한 epsilon
			R[i][1] = Rotation.GetRightVector().Data[i]; 
			absR[i][1] = std::abs(R[i][1]) + 1e-6f;
			R[i][2] = Rotation.GetUpVector().Data[i]; 
			absR[i][2] = std::abs(R[i][2]) + 1e-6f;
		}
		
		// OBB 중심과 AABB 중심 간의 변위 L
		FVector L = Center - AABB.GetCenter();
		float ra, rb;

		// AABB의 면 법선 축 (3개) 검사
		for (int i = 0; i < 3; i++)
		{
			ra = AABB.GetExtent().Data[i];
			rb = Extent.Data[0] * absR[i][0] 
				+ Extent.Data[1] * absR[i][1] 
				+ Extent.Data[2] * absR[i][2];
			if (std::abs(L.Data[i]) > ra + rb) return false;
		}
		
		// 15개 모두 검사하지 않고 이쯤에서 return true하기도 함.
		// AABB를 그냥 사용하는 것보다 pruning이 '관대'해지지만
		// 대신 연산 이득이 커서 False negative를 감수

		// OBB의 면 법선 축 (3개) 검사
		for (int i = 0; i < 3; i++)
		{
			ra = AABB.GetExtent().X * absR[0][i] 
				+ AABB.GetExtent().Y * absR[1][i] 
				+ AABB.GetExtent().Z * absR[2][i];
			rb = Extent.Data[i];
			float distance = std::abs(L.X * R[0][i] + L.Y * R[1][i] + L.Z * R[2][i]);
			if (distance > ra + rb) return false;
		}
		
		// 각 변의 외적 축 (9개) 검사
		FVector obbBase[3] = { Rotation.GetForwardVector(), Rotation.GetRightVector(), Rotation.GetUpVector() };
		FVector aabbBase[3] = { FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1) };
		for (int i=0; i<3; i++)
		{
			for (int j=0; i<3; j++)
			{
				FVector axis = obbBase[i].Cross(aabbBase[j]);
				ra = AABB.GetExtent().Dot(axis);
				rb = Extent.Dot(axis);
				float distance = L.Dot(axis);
				if (distance > ra + rb) return false;
			}
		}
		
		return true;
	}
};
