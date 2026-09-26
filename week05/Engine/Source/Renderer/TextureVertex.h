#pragma once

#include "CoreMinimal.h"

struct ENGINE_API FTextureVertex
{
	FVector Position;
	FVector2 UV;

	FTextureVertex()
		: Position(), UV()
	{
	}

	FTextureVertex(const FVector& InPosition, const FVector2& InUV)
		: Position(InPosition), UV(InUV)
	{
	}
};