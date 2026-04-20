#pragma once

#include "Render/Types/ViewTypes.h"

/*
    뷰 모드를 실제 셰이딩 모델과 파이프라인 단계로 해석할 때 사용하는 공용 타입 모음입니다.
    ViewMode -> ShadingModel 변환 규칙도 이 파일에서 함께 관리합니다.
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

enum class EPipelineStage : uint8
{
    BaseDraw = 0,
    Decal,
    Lighting,
    Count
};

inline EShadingModel GetShadingModelFromViewMode(EViewMode ViewMode)
{
    switch (ViewMode)
    {
    case EViewMode::Wireframe:
        return EShadingModel::Unlit;
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

inline bool IsLitShadingModel(EShadingModel Model)
{
    return Model != EShadingModel::Unlit;
}

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
