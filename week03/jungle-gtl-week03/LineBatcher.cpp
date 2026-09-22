#include "LineBatcher.h"
void FLineBatcher::AddLine(const FVector& start, const FVector& end, FColor Color)
{
	LineVertices.push_back({ start.x,start.y,start.z, Color.r,Color.g,Color.b,Color.a });
	LineVertices.push_back({ end.x,end.y,end.z, Color.r,Color.g,Color.b,Color.a });
}


void FLineBatcher::AddAABB(const FAABB& aabb, FColor Color)
{
	FVector corners[8] = {
		aabb.MinPos,
		FVector(aabb.MaxPos.x, aabb.MinPos.y, aabb.MinPos.z),
		FVector(aabb.MaxPos.x, aabb.MaxPos.y, aabb.MinPos.z),
		FVector(aabb.MinPos.x, aabb.MaxPos.y, aabb.MinPos.z),
		FVector(aabb.MinPos.x, aabb.MinPos.y, aabb.MaxPos.z),
		FVector(aabb.MaxPos.x, aabb.MinPos.y, aabb.MaxPos.z),
		aabb.MaxPos,
		FVector(aabb.MinPos.x, aabb.MaxPos.y, aabb.MaxPos.z)
	};

	for (int i = 0; i < 8; ++i)
	{
		corners[i] = aabb.RelativeMatrix.TransformPoint(corners[i]);
	}

	AddLine(corners[0], corners[1], Color);
	AddLine(corners[1], corners[2], Color);
	AddLine(corners[2], corners[3], Color);
	AddLine(corners[3], corners[0], Color);
	AddLine(corners[4], corners[5], Color);
	AddLine(corners[5], corners[6], Color);
	AddLine(corners[6], corners[7], Color);
	AddLine(corners[7], corners[4], Color);
	AddLine(corners[0], corners[4], Color);
	AddLine(corners[1], corners[5], Color);
	AddLine(corners[2], corners[6], Color);
	AddLine(corners[3], corners[7], Color);
}
