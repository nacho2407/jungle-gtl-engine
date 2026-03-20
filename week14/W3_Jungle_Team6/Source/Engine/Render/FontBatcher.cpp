#include "FontBatcher.h"

void FFontBatcher::Create(ID3D11Device* Device)
{
}

void FFontBatcher::Release()
{
}

void FFontBatcher::AddText(const FString& Text,
	const FVector& WorldPos,
	const FVector& CamRight,
	const FVector& CamUp,
	const FColor& Color,
	float Scale)
{
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj)
{
}

uint32 FFontBatcher::GetQuadCount() const
{
	return static_cast<uint32>(Vertices.size() / 4);
}

void FFontBatcher::GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	OutUVMin = FVector2(0.0f, 0.0f);
	OutUVMax = FVector2(1.0f, 1.0f);
}
