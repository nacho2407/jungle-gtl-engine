#pragma once

#include "Primitive/PrimitiveBase.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

struct FBatchedLine
{
	FVector Start;
	FVector End;
	FVector4 Color;
	uint32 BatchID;

	FBatchedLine()
		: Start(FVector::ZeroVector)
		, End(FVector::ZeroVector)
		, Color(1, 1, 1, 1)
		, BatchID(0)
	{
	}

	FBatchedLine(const FVector& InStart, const FVector& InEnd, const FVector4 InColor, const int InBatchID = 0)
		: Start(InStart)
		, End(InEnd)
		, Color(InColor)
		, BatchID(InBatchID)
	{
	}
};

/// <summary>
/// 런타임에 동적으로 생성된 선분들을 보관하고 있는 Primitive
/// </summary>
class ENGINE_API CPrimitiveLineBatch : public CPrimitiveBase
{
public:
	CPrimitiveLineBatch();

	/// <returns>생성한 라인의 id, 수동으로 삭제할 때 사용</returns>
	uint32 AddLine(FVector InStart, FVector InEnd, FVector4 InColor, uint32 InBatchID = 0);
	void ClearVertices();
};