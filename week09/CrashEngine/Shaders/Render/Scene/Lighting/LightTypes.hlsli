/*
    LightTypes.hlsli
    조명 계산에서 공통으로 쓰는 보조 구조체를 정의하는 헤더입니다.
    현재는 로컬 라이트 Blinn-Phong 계산 결과를 담는 `FLocalBlinnPhongTerm`를 제공합니다.

    참고 바인딩
    - 이 파일은 `ConstantBuffers.hlsl`를 통해 프레임/라이트 상수 버퍼 정의를 참조합니다.
    - 직접 텍스처나 샘플러 슬롯을 선언하지는 않습니다.
*/

#ifndef LIGHT_TYPES_HLSLI
#define LIGHT_TYPES_HLSLI

#include "../../../Resources/ConstantBuffers.hlsl"

struct FLocalBlinnPhongTerm
{
    float3 Diffuse;
    float3 Specular;
};

#endif
