#include "Math/FMatrix.h"

FMatrix FMatrix::operator*(const FMatrix& rhs) const
{
	FMatrix result{};

	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			result.M[row][col] =
				M[row][0] * rhs.M[0][col] +
				M[row][1] * rhs.M[1][col] +
				M[row][2] * rhs.M[2][col] +
				M[row][3] * rhs.M[3][col];
		}
	}

	return result;
}