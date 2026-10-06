#pragma once

#include "Materials/Graph/MaterialHlslGenerator.h"

class FMaterialGraphCompiler
{
public:
	static bool Compile(const FMaterialGraph& Graph, const FMaterialCompileOptions& Options, FMaterialCompileResult& OutResult);
};
