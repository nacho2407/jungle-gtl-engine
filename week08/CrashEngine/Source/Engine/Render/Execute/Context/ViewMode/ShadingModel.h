#pragma once

#include "Render/Execute/Context/Scene/ViewTypes.h"

/*
    뷰 모드가 사용할 장면 셰이딩 모델을 정의합니다.
    렌더 패스는 이 값을 기준으로 필요한 GBuffer와 라이팅 경로를 선택합니다.
*/
enum class EShadingModel : uint8
{
    Gouraud = 0,
    Lambert,
    BlinnPhong,
    Unlit,
    WorldNormal,
    Count
};

/*
    에디터 뷰 모드를 셰이딩 모델로 변환합니다.
*/
inline EShadingModel GetShadingModelFromViewMode(EViewMode ViewMode)
{
    switch (ViewMode)
    {
    case EViewMode::Wireframe:
    case EViewMode::SceneDepth:
        return EShadingModel::Unlit;
    case EViewMode::Lit_Lambert:
        return EShadingModel::Lambert;
    case EViewMode::Lit_Phong:
        return EShadingModel::BlinnPhong;
    case EViewMode::Unlit:
        return EShadingModel::Unlit;
    case EViewMode::WorldNormal:
        return EShadingModel::WorldNormal;
    default:
        return EShadingModel::Gouraud;
    }
}

/*
    별도 라이팅 패스가 필요한 셰이딩 모델인지 확인합니다.
*/
inline bool IsLitShadingModel(EShadingModel Model)
{
    return Model == EShadingModel::Gouraud || Model == EShadingModel::Lambert || Model == EShadingModel::BlinnPhong;
}

/*
    디버그 이름이나 마커에 사용할 셰이딩 모델 이름을 반환합니다.
*/
inline const char* GetShadingModelName(EShadingModel Model)
{
    switch (Model)
    {
    case EShadingModel::Gouraud:
        return "Gouraud";
    case EShadingModel::Lambert:
        return "Lambert";
    case EShadingModel::BlinnPhong:
        return "BlinnPhong";
    case EShadingModel::Unlit:
        return "Unlit";
    case EShadingModel::WorldNormal:
        return "WorldNormal";
    default:
        return "Unknown";
    }
}
