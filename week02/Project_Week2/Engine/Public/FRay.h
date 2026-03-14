#pragma once
#include "FVector.h"

struct FRay 
{
	FVector Origin; // 광선의 시작점
	FVector Direction; // 광선의 방향 벡터 (정규화된)
};