#pragma once
#include "CoreMinimal.h"
#include "EngineAPI.h"
#include "Math/Vector.h"
#include "../Math/Vector.h"

class FRenderer;
class FShowFlags;
class UWorld;
struct FDebugLine
{
    FVector Start;
    FVector End;
    FVector4 Color;
};

struct FDebugCube
{
    FVector Center;
    FVector Extent;
    FVector4 Color;
};

class ENGINE_API FDebugDrawManager
{
public:
	void DrawLine(const FVector& Start, const FVector& End, const FVector4& Color);
	void DrawCube(const FVector& Center, const FVector& Extent, const FVector4& Color);
	void DrawWorldAxis(float Length = 1000.f);

	void Flush(FRenderer* Renderer, const FShowFlags& ShowFlags, UWorld* World);
	void Clear();
private:
	TArray<FDebugLine> Lines;
	TArray<FDebugCube> Cubes;
	bool bDrawWorldAxis = false;
	void DrawAllCollisionBounds(FRenderer* Renderer, UWorld* World);
};