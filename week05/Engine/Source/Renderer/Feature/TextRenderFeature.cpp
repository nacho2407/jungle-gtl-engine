#include "Renderer/Feature/TextRenderFeature.h"

#include "Renderer/Renderer.h"

bool FTextRenderFeature::Initialize(FRenderer& Renderer)
{
	return TextMeshBuilder.Initialize(&Renderer);
}

void FTextRenderFeature::Release()
{
	TextMeshBuilder.Release();
}

FMaterial* FTextRenderFeature::GetBaseMaterial() const
{
	return TextMeshBuilder.GetFontMaterial();
}

bool FTextRenderFeature::BuildMesh(
	const FString& Text,
	FRenderMesh& OutMesh,
	float LetterSpacing,
	EHorizTextAligment HorizAlignment,
	EVerticalTextAligment VertAlignment) const
{
	return TextMeshBuilder.BuildTextMesh(Text, OutMesh, LetterSpacing, HorizAlignment, VertAlignment);
}
