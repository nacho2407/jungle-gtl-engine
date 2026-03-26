#pragma once
#include "Math/Vector.h"
#include "Singleton.h"
#include "EngineCore/EngineTypes.h"
#include "Math/Matrix.h"
#include "AABB.h"
#include "EngineCore/Color.h"
#include "VertexSimple.h"
struct FLineBatcher : public ISingleton<FLineBatcher>
{
public:
	FLineBatcher() {}
	~FLineBatcher() {}
	void AddLine(const FVector& start, const FVector& end, FColor Color);
	void AddAABB(const FAABB& aabb, FColor Color);
	const TArray<FVertexSimple>& GetLineVertices() const { return LineVertices; }
	void ClearLines() { LineVertices.clear(); }

private:
	TArray<FVertexSimple> LineVertices;
};
