#pragma once

#include "Render/Common/ComPtr.h"

struct ID3D11ShaderResourceView;

struct FCascadeToolbarIcons
{
	bool bLoadAttempted = false;

	TComPtr<ID3D11ShaderResourceView> SaveIcon;
	TComPtr<ID3D11ShaderResourceView> FindIcon;
	TComPtr<ID3D11ShaderResourceView> RestartSimIcon;
	TComPtr<ID3D11ShaderResourceView> RestartLevelIcon;
	TComPtr<ID3D11ShaderResourceView> UndoIcon;
	TComPtr<ID3D11ShaderResourceView> RedoIcon;
	TComPtr<ID3D11ShaderResourceView> BoundsIcon;
	TComPtr<ID3D11ShaderResourceView> AxisIcon;
	TComPtr<ID3D11ShaderResourceView> BackgroundIcon;
	TComPtr<ID3D11ShaderResourceView> ThumbnailIcon;
	TComPtr<ID3D11ShaderResourceView> RegenLODIcon;
	TComPtr<ID3D11ShaderResourceView> LowestLODIcon;
	TComPtr<ID3D11ShaderResourceView> HighestLODIcon;
	TComPtr<ID3D11ShaderResourceView> LowerLODIcon;
	TComPtr<ID3D11ShaderResourceView> UpperLODIcon;
	TComPtr<ID3D11ShaderResourceView> AddLODIcon;
	TComPtr<ID3D11ShaderResourceView> DeleteLODIcon;
	TComPtr<ID3D11ShaderResourceView> GenericLODIcon;
	TComPtr<ID3D11ShaderResourceView> CurveHorizontalIcon;
	TComPtr<ID3D11ShaderResourceView> CurveVerticalIcon;
	TComPtr<ID3D11ShaderResourceView> CurveFitIcon;
	TComPtr<ID3D11ShaderResourceView> CurvePanIcon;
	TComPtr<ID3D11ShaderResourceView> CurveZoomIcon;
	TComPtr<ID3D11ShaderResourceView> CurveAutoIcon;
	TComPtr<ID3D11ShaderResourceView> CurveAutoClampedIcon;
	TComPtr<ID3D11ShaderResourceView> CurveUserIcon;
	TComPtr<ID3D11ShaderResourceView> CurveBreakIcon;
	TComPtr<ID3D11ShaderResourceView> CurveLinearIcon;
	TComPtr<ID3D11ShaderResourceView> CurveConstantIcon;
	TComPtr<ID3D11ShaderResourceView> CurveFlattenIcon;
	TComPtr<ID3D11ShaderResourceView> CurveStraightenIcon;
	TComPtr<ID3D11ShaderResourceView> CurveShowAllIcon;
	TComPtr<ID3D11ShaderResourceView> CurveCreateIcon;
	TComPtr<ID3D11ShaderResourceView> CurveDeleteIcon;
};
