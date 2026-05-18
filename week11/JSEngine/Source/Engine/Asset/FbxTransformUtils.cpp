#include "FbxTransformUtils.h"
#include <fbxsdk.h>
FVector FFbxTransformUtils::ToFVector(const fbxsdk::FbxVector4& Vector)
{
	return FVector(static_cast<float>(Vector[0]), static_cast<float>(Vector[1]), static_cast<float>(Vector[2]));
}

FQuat FFbxTransformUtils::ToFQuat(const fbxsdk::FbxQuaternion& Quat)
{
    FQuat Result(static_cast<float>(Quat[0]), static_cast<float>(Quat[1]), static_cast<float>(Quat[2]), static_cast<float>(Quat[3]));

    Result.Normalize();
    return Result;
}

FMatrix FFbxTransformUtils::ToFMatrix(const fbxsdk::FbxAMatrix& Matrix)
{
    return FMatrix(
        static_cast<float>(Matrix.Get(0, 0)), static_cast<float>(Matrix.Get(0, 1)), static_cast<float>(Matrix.Get(0, 2)), static_cast<float>(Matrix.Get(0, 3)),
        static_cast<float>(Matrix.Get(1, 0)), static_cast<float>(Matrix.Get(1, 1)), static_cast<float>(Matrix.Get(1, 2)), static_cast<float>(Matrix.Get(1, 3)),
        static_cast<float>(Matrix.Get(2, 0)), static_cast<float>(Matrix.Get(2, 1)), static_cast<float>(Matrix.Get(2, 2)), static_cast<float>(Matrix.Get(2, 3)),
        static_cast<float>(Matrix.Get(3, 0)), static_cast<float>(Matrix.Get(3, 1)), static_cast<float>(Matrix.Get(3, 2)), static_cast<float>(Matrix.Get(3, 3)));

}
