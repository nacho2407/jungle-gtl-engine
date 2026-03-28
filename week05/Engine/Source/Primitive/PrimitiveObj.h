#pragma once

#include "PrimitiveBase.h"

class CPrimitiveObj : public CPrimitiveBase
{
public:
	CPrimitiveObj();
	CPrimitiveObj(const FString& FilePath);

private:
	void LoadObj(const FString& FilePath);
};