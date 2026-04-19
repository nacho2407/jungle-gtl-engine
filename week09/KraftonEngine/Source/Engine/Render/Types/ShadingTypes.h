#pragma once

#include "Render/Types/ViewTypes.h"

enum class EShadingModel : uint8
{
	Gouraud = 0,
	Lambert,
	BlinnPhong,
	Unlit,
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
	case EViewMode::Wireframe:    return EShadingModel::Unlit;
	case EViewMode::SceneDepth:   return EShadingModel::Unlit;
	case EViewMode::Lit_Lambert: return EShadingModel::Lambert;
	case EViewMode::Lit_Phong:   return EShadingModel::BlinnPhong;
	case EViewMode::Normal:      return EShadingModel::Lambert;
	case EViewMode::Unlit:       return EShadingModel::Unlit;
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
	case EShadingModel::Gouraud:    return "Gouraud";
	case EShadingModel::Lambert:    return "Lambert";
	case EShadingModel::BlinnPhong: return "BlinnPhong";
	case EShadingModel::Unlit:      return "Unlit";
	default:                        return "Unknown";
	}
}
