#pragma once

#include "Core/CoreTypes.h"

/*
	렌더 파이프라인 상태(DepthStencil, Blend, Rasterizer)에 사용되는 enum 정의입니다.
*/

enum class EDepthStencilState
{
	Default,
	DepthGreaterEqual,	// Reversed-Z: GREATER_EQUAL + depth write (PreDepth 후 Opaque에서 Early-Z용)
	DepthReadOnly,
	StencilWrite,
	StencilWriteOnlyEqual,
	NoDepth,

	// --- 기즈모 전용 ---
	GizmoInside,
	GizmoOutside,
	MAX
};

enum class EBlendState
{
	Opaque,
	AlphaBlend,
	Additive,
	NoColor,
	MAX
};

enum class ERasterizerState
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
	MAX
};

// ============================================================
// Enum ↔ String 변환 테이블
// ============================================================

namespace RenderStateStrings
{
	struct FEnumEntry
	{
		const char* Str;
		int Value;
	};

	inline constexpr FEnumEntry BlendStateMap[] =
	{
		{ "Opaque",     (int)EBlendState::Opaque },
		{ "AlphaBlend", (int)EBlendState::AlphaBlend },
		{ "Additive",   (int)EBlendState::Additive },
		{ "NoColor",    (int)EBlendState::NoColor },
	};

	inline constexpr FEnumEntry DepthStencilStateMap[] =
	{
		{ "Default",              (int)EDepthStencilState::Default },
		{ "DepthGreaterEqual",    (int)EDepthStencilState::DepthGreaterEqual },
		{ "DepthReadOnly",        (int)EDepthStencilState::DepthReadOnly },
		{ "StencilWrite",         (int)EDepthStencilState::StencilWrite },
		{ "StencilWriteOnlyEqual",(int)EDepthStencilState::StencilWriteOnlyEqual },
		{ "NoDepth",              (int)EDepthStencilState::NoDepth },
		{ "GizmoInside",          (int)EDepthStencilState::GizmoInside },
		{ "GizmoOutside",         (int)EDepthStencilState::GizmoOutside },
	};

	inline constexpr FEnumEntry RasterizerStateMap[] =
	{
		{ "SolidBackCull",  (int)ERasterizerState::SolidBackCull },
		{ "SolidFrontCull", (int)ERasterizerState::SolidFrontCull },
		{ "SolidNoCull",    (int)ERasterizerState::SolidNoCull },
		{ "WireFrame",      (int)ERasterizerState::WireFrame },
	};

	static_assert(ARRAYSIZE(BlendStateMap)        == (int)EBlendState::MAX,        "BlendStateMap must match EBlendState entries");
	static_assert(ARRAYSIZE(DepthStencilStateMap) == (int)EDepthStencilState::MAX, "DepthStencilStateMap must match EDepthStencilState entries");
	static_assert(ARRAYSIZE(RasterizerStateMap)   == (int)ERasterizerState::MAX,   "RasterizerStateMap must match ERasterizerState entries");

	// String → Enum
	template<typename EnumT, size_t N>
	inline EnumT FromString(const FEnumEntry (&Map)[N], const FString& Str, EnumT Default)
	{
		for (size_t i = 0; i < N; ++i)
		{
			if (Str == Map[i].Str)
				return static_cast<EnumT>(Map[i].Value);
		}
		return Default;
	}

	// Enum → String
	template<typename EnumT, size_t N>
	inline const char* ToString(const FEnumEntry (&Map)[N], EnumT Value)
	{
		for (size_t i = 0; i < N; ++i)
		{
			if (static_cast<EnumT>(Map[i].Value) == Value)
				return Map[i].Str;
		}
		return "";
	}
}
