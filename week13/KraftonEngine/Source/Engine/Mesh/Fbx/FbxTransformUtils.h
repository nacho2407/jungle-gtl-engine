#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"

#include <fbxsdk.h>

class FFbxTransformUtils
{
public:
	static FMatrix ToEngineMatrix(const FbxMatrix& Matrix);
	static FMatrix ToEngineMatrix(const FbxAMatrix& Matrix);
	static FMatrix ToEngineInverseMatrix(const FbxAMatrix& Matrix);
	static FbxAMatrix GetGeometryTransform(FbxNode* Node);
};
