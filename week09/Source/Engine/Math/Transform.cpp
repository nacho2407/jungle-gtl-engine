// 수학 영역의 세부 동작을 구현합니다.
#include "Transform.h"

FMatrix FTransform::ToMatrix() const
{
    FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

    FMatrix rotationMatrix = Rotation.ToMatrix();

    FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

    return scaleMatrix * rotationMatrix * translateMatrix;
}
