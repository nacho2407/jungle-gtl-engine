#include "TextMeshComponent.h"

#include "EngineCore/ResourceManager.h"
#include "Text/FontAsset.h"
#include "Text/TextEncoding.h"

UTextMeshComponent::UTextMeshComponent()
{
	SetType(EPrimitiveType::TextMesh);
	VResource = &TextGeometry;
	SetRelativeScale3D(FVector(1.0f, 0.05f, 0.05f));
	RebuildTextGeometry();
	UpdateAABB();
}

void UTextMeshComponent::OnComponentAdded()
{
	RebuildTextGeometry();
	UPrimitiveComponent::OnComponentAdded();
}

void UTextMeshComponent::Update(float DeltaTime)
{
	UPrimitiveComponent::Update(DeltaTime);
}

json::JSON UTextMeshComponent::Serialize()
{
	json::JSON JsonObj = UPrimitiveComponent::Serialize();
	JsonObj["Type"] = "TextMesh";
	JsonObj["Text"] = Text;
	return JsonObj;
}

void UTextMeshComponent::Deserialize(json::JSON JsonObj)
{
	USceneComponent::Deserialize(JsonObj);
	if (JsonObj.hasKey("Text"))
	{
		Text = JsonObj["Text"].ToString();
	}

	RebuildTextGeometry();
	UpdateRenderObject();
	UpdateAABB();
}

void UTextMeshComponent::SetText(const FString& InText)
{
	if (Text == InText)
	{
		return;
	}

	Text = InText;
	RebuildTextGeometry();
	UpdateAABB();
	MarkRenderStateDirty();
}

RenderObject* UTextMeshComponent::CreateRenderObject()
{
	RenderObject* RenderObjectInstance = UPrimitiveComponent::CreateRenderObject();
	if (RenderObjectInstance == nullptr)
	{
		return nullptr;
	}

	ResourceManager* ResourceMgr = ResourceManager::GetInstance();
	RenderObjectInstance->Material = &ResourceMgr->GetMaterial(L"Shaders/ShaderTextMesh.hlsl");
	RenderObjectInstance->ShowFlag = EEngineShowFlags::SF_Primitives;
	RenderObjectInstance->bDepthEnabled = true;
	RenderObjectInstance->bDisableCulling = false;
	RenderObjectInstance->Geometry = &TextGeometry;
	UpdateRenderObject();
	return RenderObjectInstance;
}

void UTextMeshComponent::UpdateRenderObject()
{
	if (RenderObj == nullptr)
	{
		return;
	}

	RenderObj->World = GetRelativeMatrix();
	RenderObj->bIsSelected = bIsSelected;
	RenderObj->ShowFlag = EEngineShowFlags::SF_Primitives;
	RenderObj->Geometry = &TextGeometry;
	RenderObj->bDisableCulling = false;
}

void UTextMeshComponent::RebuildTextGeometry()
{
	TextVertices.clear();
	TextGeometry.Vertices.clear();
	TextGeometry.Indices.clear();
	TextGeometry.Colors.clear();
	TextGeometry.TexCoords.clear();
	TextGeometry.VertexCount = 0;

	ResourceManager* ResourceMgr = ResourceManager::GetInstance();
	FFontAsset& FontAsset = ResourceMgr->GetBillboardFont();

	if (Text.empty())
	{
		TextGeometry.VertexStride = sizeof(FVertexSimple);
		VResource = &TextGeometry;
		return;
	}

	if (!ResourceMgr->EnsureBillboardGlyphs(Text))
	{
		TextGeometry.VertexStride = sizeof(FVertexSimple);
		VResource = &TextGeometry;
		return;
	}

	const FU32String Codepoints = Utf8ToCodepoints(Text);

	float PenY = 0.0f;
	float PenZ = 0.0f;

	for (uint32 Codepoint : Codepoints)
	{
		if (Codepoint == U'\r')
		{
			continue;
		}

		if (Codepoint == U'\n')
		{
			PenY = 0.0f;
			PenZ -= FontAsset.GetFontPixelHeight();
			continue;
		}

		const FGlyphInfo* Glyph = FontAsset.EnsureGlyph(Codepoint);
		if (Glyph == nullptr)
		{
			continue;
		}

		if (Glyph->Metrics.Width <= 0.0f || Glyph->Metrics.Height <= 0.0f)
		{
			PenY += Glyph->Metrics.AdvanceX;
			continue;
		}

		const float GlyphY0 = PenY + Glyph->Metrics.BearingX;
		const float GlyphY1 = GlyphY0 + Glyph->Metrics.Width;
		const float GlyphZ1 = PenZ + Glyph->Metrics.BearingY;
		const float GlyphZ0 = GlyphZ1 - Glyph->Metrics.Height;

		const FVertexSimple V0 = { 0.0f, GlyphY0, GlyphZ1, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U0, Glyph->V0 };
		const FVertexSimple V1 = { 0.0f, GlyphY1, GlyphZ1, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U1, Glyph->V0 };
		const FVertexSimple V2 = { 0.0f, GlyphY0, GlyphZ0, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U0, Glyph->V1 };
		const FVertexSimple V3 = { 0.0f, GlyphY1, GlyphZ1, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U1, Glyph->V0 };
		const FVertexSimple V4 = { 0.0f, GlyphY1, GlyphZ0, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U1, Glyph->V1 };
		const FVertexSimple V5 = { 0.0f, GlyphY0, GlyphZ0, TextColor.r, TextColor.g, TextColor.b, TextColor.a, Glyph->U0, Glyph->V1 };

		TextVertices.push_back(V0);
		TextVertices.push_back(V1);
		TextVertices.push_back(V2);
		TextVertices.push_back(V3);
		TextVertices.push_back(V4);
		TextVertices.push_back(V5);

		TextGeometry.Vertices.push_back(FVector(V0.x, V0.y, V0.z));
		TextGeometry.Vertices.push_back(FVector(V1.x, V1.y, V1.z));
		TextGeometry.Vertices.push_back(FVector(V2.x, V2.y, V2.z));
		TextGeometry.Vertices.push_back(FVector(V3.x, V3.y, V3.z));
		TextGeometry.Vertices.push_back(FVector(V4.x, V4.y, V4.z));
		TextGeometry.Vertices.push_back(FVector(V5.x, V5.y, V5.z));

		PenY += Glyph->Metrics.AdvanceX;
	}

	TextGeometry.VertexStride = sizeof(FVertexSimple);
	TextGeometry.VertexCount = static_cast<UINT>(TextGeometry.Vertices.size());
	VResource = &TextGeometry;
}

REGISTER_CLASS(UTextMeshComponent);
