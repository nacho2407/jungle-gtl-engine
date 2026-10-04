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
	FVector ToFVector(const fbxsdk::FbxVector4& Vector);
	FQuat ToFQuat(const fbxsdk::FbxQuaternion& Quat);
	FMatrix ToFMatrix(const fbxsdk::FbxAMatrix& Matrix);
} 
