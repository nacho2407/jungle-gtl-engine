#include "LineBatcher.h"

void FLineBatcher::Create(ID3D11Device* Device)
{
}

void FLineBatcher::Release()
{
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FColor& Color)
{
}

void FLineBatcher::AddAABB(const FBoundingBox& Box, const FColor& Color)
{
}

void FLineBatcher::AddWorldGrid(float GridSpacing, int HalfLineCount)
{
}

void FLineBatcher::Clear()
{
	Vertices.clear();
}

void FLineBatcher::Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj)
{
}

uint32 FLineBatcher::GetLineCount() const
{
	return static_cast<uint32>(Vertices.size() / 2);
}
