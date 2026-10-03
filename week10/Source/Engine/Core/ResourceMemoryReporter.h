#pragma once

#include <cstddef>

class FMaterialResourceCache;

class FResourceMemoryReporter
{
public:
	static size_t GetMaterialMemorySize(const FMaterialResourceCache& MaterialCache);
};
