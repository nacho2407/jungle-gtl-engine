#include "Vector4.h"
#include "Matrix.h"

inline FVector4 FVector4::operator*(const FMatrix& Mat) const noexcept
{
	const DirectX::XMVECTOR R = DirectX::XMVector4Transform(ToXMVector(), Mat.ToXMMatrix());
	DirectX::XMFLOAT4 T;
	DirectX::XMStoreFloat4(&T, R);
	return FVector4(T.x, T.y, T.z, T.w);

	// FVector4 NewVec4;
	//
	// for (int32 Col = 0; Col < 4; Col++)
	// {
	//     // Original (buggy) implementation preserved:
	//     // NewVec4.XYZW[Col] =
	//     //     X * Mat.M[0][Col] + Y * Mat.M[1][Col] +
	//     //                     Z * Mat.M[2][Col] * W * Mat.M[3][Col];
	//     //
	//     NewVec4.XYZW[Col] =
	//         X * Mat.M[0][Col] + Y * Mat.M[1][Col] +
	//         Z * Mat.M[2][Col] + W * Mat.M[3][Col];
	// }
	// return NewVec4;
}