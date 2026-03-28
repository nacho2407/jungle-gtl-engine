#pragma once

#include "Core/CoreTypes.h"
#include "Math/Color.h"
#include "Math/AABB.h"
// #include "Math/Vector.h"
//
// // ============================================================
// // FColor — RGBA 색상 (0~255 정수 기반, 셰이더 전달 시 Normalized)
// // ============================================================
// struct FColor
// {
// 	uint32 R, G, B, A;
//
// 	FColor() : R(255), G(255), B(255), A(255) {}
// 	FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA = 255)
// 		: R(InR), G(InG), B(InB), A(InA) {
// 	}
//
// 	// 0.0~1.0 범위의 FVector4로 변환
// 	FVector4 ToVector4() const
// 	{
// 		return FVector4(R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f);
// 	}
//
// 	static FColor Red() { return FColor(255, 0, 0); }
// 	static FColor Green() { return FColor(0, 255, 0); }
// 	static FColor Blue() { return FColor(0, 0, 255); }
// 	static FColor White() { return FColor(255, 255, 255); }
// 	static FColor Black() { return FColor(0, 0, 0); }
// 	static FColor Yellow() { return FColor(255, 255, 0); }
// 	static FColor Gray() { return FColor(139, 139, 139); }
// };

// ============================================================
// FBoundingBox — AABB (Axis-Aligned Bounding Box)
// ============================================================
// struct FBoundingBox
// {
// 	FVector Min;
// 	FVector Max;
//
// 	FBoundingBox()
// 		: Min(FVector(FLT_MAX, FLT_MAX, FLT_MAX))
// 		, Max(FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX))
// 	{
// 	}
//
// 	FBoundingBox(const FVector& InMin, const FVector& InMax)
// 		: Min(InMin), Max(InMax) {
// 	}
//
// 	// 점을 포함하도록 확장
// 	void Expand(const FVector& Point);
//
// 	// 중심점 반환
// 	FVector GetCenter() const;
//
// 	// 크기(Extent) 반환
// 	FVector GetExtent() const;
//
// 	// 유효 여부 (Min < Max)
// 	bool IsValid() const;
// };

//	TODO : 나중에 지우기
using FBoundingBox = FAABB;

// ============================================================
// EViewModeIndex — 렌더링 뷰 모드
// ============================================================
enum class EViewModeIndex
{
	Lit,        // 기본 라이팅 적용
	Unlit,      // 라이팅 없음
	Wireframe   // 와이어프레임
};

// ============================================================
// EEngineShowFlags — 렌더링 요소별 On/Off 비트 플래그
// ============================================================
enum EEngineShowFlags : uint32
{
	SF_None = 0,
	SF_Primitives = 1 << 0,   // 프리미티브 메시
	SF_Grid = 1 << 1,   // 월드 그리드
	SF_BoundingBox = 1 << 2,   // AABB 바운딩 박스
	SF_BillboardText = 1 << 3,   // 빌보드 UUID 텍스트
	SF_Gizmo = 1 << 4,   // 트랜스폼 기즈모

	SF_All = SF_Primitives | SF_Grid | SF_BoundingBox | SF_BillboardText | SF_Gizmo
};
