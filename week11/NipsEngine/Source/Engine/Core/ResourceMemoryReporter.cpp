#include "Core/ResourceMemoryReporter.h"

#include "Core/MaterialResourceCache.h"

size_t FResourceMemoryReporter::GetMaterialMemorySize(const FMaterialResourceCache& MaterialCache)
{
	return MaterialCache.GetMaterialMemorySize();
}
