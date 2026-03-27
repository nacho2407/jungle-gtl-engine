#pragma once


#include "Core/CoreTypes.h"

class IAssetLoader
{
public:
	virtual ~IAssetLoader() = default;

	//	지원하는 확장자 여부 반환
	virtual bool SupportsExtension(const FString& Extesion) const = 0;

	virtual FString GetLoaderName() const = 0;
};