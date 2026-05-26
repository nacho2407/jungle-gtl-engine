#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ShaderPaths.h"

enum class EMaterialShaderType : uint8
{
	None,
	SurfaceLit,
	Translucent,
	Decal,
	UIFont,
	UILine,
	VFXSubUV,
	VFXParticle,
	EditorGizmo,
	EditorOutline,
};

inline const FString& ToString(EMaterialShaderType Type)
{
	static const FString None = "None";
	static const FString SurfaceLit = "SurfaceLit";
	static const FString Translucent = "Translucent";
	static const FString Decal = "Decal";
	static const FString UIFont = "UIFont";
	static const FString UILine = "UILine";
	static const FString VFXSubUV = "VFXSubUV";
	static const FString VFXParticle = "VFXParticle";
	static const FString EditorGizmo = "EditorGizmo";
	static const FString EditorOutline = "EditorOutline";

	switch (Type)
	{
	case EMaterialShaderType::None:
		return None;
	case EMaterialShaderType::Translucent:
		return Translucent;
	case EMaterialShaderType::Decal:
		return Decal;
	case EMaterialShaderType::UIFont:
		return UIFont;
	case EMaterialShaderType::UILine:
		return UILine;
	case EMaterialShaderType::VFXSubUV:
		return VFXSubUV;
	case EMaterialShaderType::VFXParticle:
		return VFXParticle;
	case EMaterialShaderType::EditorGizmo:
		return EditorGizmo;
	case EMaterialShaderType::EditorOutline:
		return EditorOutline;
	case EMaterialShaderType::SurfaceLit:
	default:
		return SurfaceLit;
	}
}

inline bool TryParseMaterialShaderType(const FString& Name, EMaterialShaderType& OutType)
{
	if (Name == "None")
	{
		OutType = EMaterialShaderType::None;
		return true;
	}
	if (Name == "SurfaceLit")
	{
		OutType = EMaterialShaderType::SurfaceLit;
		return true;
	}
	if (Name == "Translucent" || Name == "Transparent")
	{
		OutType = EMaterialShaderType::Translucent;
		return true;
	}
	if (Name == "Decal")
	{
		OutType = EMaterialShaderType::Decal;
		return true;
	}
	if (Name == "UIFont")
	{
		OutType = EMaterialShaderType::UIFont;
		return true;
	}
	if (Name == "UILine")
	{
		OutType = EMaterialShaderType::UILine;
		return true;
	}
	if (Name == "VFXSubUV")
	{
		OutType = EMaterialShaderType::VFXSubUV;
		return true;
	}
	if (Name == "VFXParticle")
	{
		OutType = EMaterialShaderType::VFXParticle;
		return true;
	}
	if (Name == "EditorGizmo")
	{
		OutType = EMaterialShaderType::EditorGizmo;
		return true;
	}
	if (Name == "EditorOutline")
	{
		OutType = EMaterialShaderType::EditorOutline;
		return true;
	}
	return false;
}

inline const FString& GetMaterialPixelShaderPath(EMaterialShaderType Type)
{
	static const FString Empty;
	static const FString MaterialUberLit = FShaderPaths::MaterialUberLit;
	static const FString MaterialUberTranslucent = FShaderPaths::MaterialUberTranslucent;
	static const FString MaterialDecal = FShaderPaths::MaterialDecal;
	static const FString UIFont = FShaderPaths::UIFont;
	static const FString UILine = FShaderPaths::UILine;
	static const FString VFXSubUV = FShaderPaths::VFXSubUV;
	static const FString VFXParticle = FShaderPaths::VFXParticle;
	static const FString EditorGizmo = FShaderPaths::EditorGizmo;
	static const FString EditorOutline = FShaderPaths::PostProcessOutline;

	switch (Type)
	{
	case EMaterialShaderType::None:
		return Empty;
	case EMaterialShaderType::Translucent:
		return MaterialUberTranslucent;
	case EMaterialShaderType::Decal:
		return MaterialDecal;
	case EMaterialShaderType::UIFont:
		return UIFont;
	case EMaterialShaderType::UILine:
		return UILine;
	case EMaterialShaderType::VFXSubUV:
		return VFXSubUV;
	case EMaterialShaderType::VFXParticle:
		return VFXParticle;
	case EMaterialShaderType::EditorGizmo:
		return EditorGizmo;
	case EMaterialShaderType::EditorOutline:
		return EditorOutline;
	case EMaterialShaderType::SurfaceLit:
	default:
		return MaterialUberLit;
	}
}

inline const FString& GetMaterialPixelShaderEntryPoint(EMaterialShaderType Type)
{
	static const FString Empty;
	static const FString MainPS = "mainPS";
	static const FString PS = "PS";

	switch (Type)
	{
	case EMaterialShaderType::None:
		return Empty;
	case EMaterialShaderType::UIFont:
	case EMaterialShaderType::UILine:
	case EMaterialShaderType::VFXSubUV:
	case EMaterialShaderType::VFXParticle:
	case EMaterialShaderType::EditorGizmo:
	case EMaterialShaderType::EditorOutline:
		return PS;
	case EMaterialShaderType::Translucent:
	case EMaterialShaderType::SurfaceLit:
	case EMaterialShaderType::Decal:
	default:
		return MainPS;
	}
}

inline bool IsTransparentMaterialShaderType(EMaterialShaderType Type)
{
	return Type == EMaterialShaderType::Translucent;
}
