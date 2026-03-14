#pragma once
#include "FVector.h"
#include "FRay.h"

struct FPlane {
	FVector Normal; // 평면의 법선 벡터
	float dis; // 원점에서의 거리

	FPlane() : Normal(0, 1, 0), dis(0) {} // 기본 평면은 XY 평면 (법선 벡터가 Y축을 향함)

	// 점과 평면 사이의 거리 구하는 함수
	float Dot(const FVector& Point) const {
		return Normal.Dot(Point) + dis;
	}

	bool Intersect(const FRay& Ray, float& OutT, FVector& OutIntersectionPoint) const;
};