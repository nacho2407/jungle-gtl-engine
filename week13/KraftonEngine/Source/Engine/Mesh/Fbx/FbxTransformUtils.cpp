#include "Mesh/Fbx/FbxTransformUtils.h"

FMatrix FFbxTransformUtils::ToEngineMatrix(const FbxMatrix& FbxMat)
{
	FMatrix Mat;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Mat.M[Row][Col] = static_cast<float>(FbxMat.Get(Row, Col));
		}
	}
	return Mat;
}

FMatrix FFbxTransformUtils::ToEngineMatrix(const FbxAMatrix& FbxMat)
{
	FMatrix Mat;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Mat.M[Row][Col] = static_cast<float>(FbxMat.Get(Row, Col));
		}
	}
	return Mat;
}

FMatrix FFbxTransformUtils::ToEngineInverseMatrix(const FbxAMatrix& FbxMat)
{
	return ToEngineMatrix(FbxMat.Inverse());
}

FbxAMatrix FFbxTransformUtils::GetGeometryTransform(FbxNode* Node)
{
	FbxAMatrix GeometryTransform;
	if (!Node)
	{
		return GeometryTransform;
	}

	GeometryTransform.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
	GeometryTransform.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
	GeometryTransform.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));
	return GeometryTransform;
}
