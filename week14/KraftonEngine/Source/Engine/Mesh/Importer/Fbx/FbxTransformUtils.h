#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Matrix.h"

#include <fbxsdk.h>

class FFbxTransformUtils
{
public:
	static FMatrix ToEngineMatrix(const FbxMatrix& Matrix);
	static FMatrix ToEngineMatrix(const FbxAMatrix& Matrix);
	static FMatrix ToEngineInverseMatrix(const FbxAMatrix& Matrix);
	static double GetBasisDeterminant(const FMatrix& Matrix);
	static bool HasNegativeBasisDeterminant(const FMatrix& Matrix);
	static FbxAMatrix GetGeometryTransform(FbxNode* Node);
};
