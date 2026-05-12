#pragma once
#include "Core/CoreTypes.h"
// 블렌더 스타일 Forward 축 선택
enum class EForwardAxis : uint8
{
	X, NegX,   // +X, -X
	Y, NegY,   // +Y, -Y
	Z, NegZ    // +Z, -Z
};

enum class EWindingOrder : uint8
{
	CCW_to_CW,  // OBJ CCW → DX CW (인덱스 [0,2,1]) — 기본값
	Keep         // 원본 유지 [0,1,2]
};


enum class EStaticFbxSkinnedMeshPolicy
{
	Skip,
	ImportBindPoseAsStatic,
};

struct FImportOptions
{
	float Scale = 1.0f;
	EForwardAxis ForwardAxis = EForwardAxis::NegY;  // Blender 기본: Z-up, -Y Forward
	EWindingOrder WindingOrder = EWindingOrder::CCW_to_CW;
	EStaticFbxSkinnedMeshPolicy StaticFbxSkinnedMeshPolicy = EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic;
	static FImportOptions Default() { return {}; }
};