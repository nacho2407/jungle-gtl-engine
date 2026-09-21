#pragma once

#include "Resource/FBuiltInMeshes.h"

class FBuiltInMeshRepository
{
public:
	bool Initialize();
	const FBuiltInMeshes& Get() const;

private:
	FBuiltInMeshes Meshes;
};