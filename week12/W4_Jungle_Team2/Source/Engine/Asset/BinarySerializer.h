#pragma once

#include "Core/CoreMinimal.h"

struct FStaticMesh;

class FBinarySerializer
{
public:
	bool SaveStaticMesh(const FString & BinaryPath, const FStaticMesh & Data);
	bool LoadStaticMesh(const FString & BinaryPath, const FStaticMesh & OutData);
};
