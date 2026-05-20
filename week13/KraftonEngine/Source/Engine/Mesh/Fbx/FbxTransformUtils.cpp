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

double FFbxTransformUtils::GetBasisDeterminant(const FMatrix& Matrix)
{
	const double A = Matrix.M[0][0];
	const double B = Matrix.M[0][1];
	const double C = Matrix.M[0][2];
	const double D = Matrix.M[1][0];
	const double E = Matrix.M[1][1];
	const double F = Matrix.M[1][2];
	const double G = Matrix.M[2][0];
	const double H = Matrix.M[2][1];
	const double I = Matrix.M[2][2];

	return A * (E * I - F * H) - B * (D * I - F * G) + C * (D * H - E * G);
}

bool FFbxTransformUtils::HasNegativeBasisDeterminant(const FMatrix& Matrix)
{
	return GetBasisDeterminant(Matrix) < 0.0;
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
