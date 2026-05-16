#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Quat.h"

namespace fbxsdk
{
	class FbxVector4;
	class FbxQuaternion;
	class FbxAMatrix;
} 

namespace FFbxTransformUtils
{
	FVector ToFVector(const fbxsdk::FbxVector4& V);
	FQuat ToFQuat(const fbxsdk::FbxQuaternion& Q);
	FMatrix ToFMatrix(const fbxsdk::FbxAMatrix& M);
} 
