#pragma once

#include "Geometry.h"

template<typename T>
struct TGeometry : public FGeometry {
	TArray<T> CpuVertices;

	//void AppendToBatch(Tarray)
};