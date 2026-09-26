#include "Renderer/ScreenUIRenderer.h"

#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderFeatureInterfaces.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>

namespace
{
	static FVector4 ToColor(uint32 C)
	{
		const float A = ((C >> 24) & 0xFF) / 255.0f;
		const float R = ((C >> 16) & 0xFF) / 255.0f;
		const float G = ((C >> 8) & 0xFF) / 255.0f;
		const float B = (C & 0xFF) / 255.0f;
		return { R, G, B, A };
	}

	static void ConvertTextMeshToScreenSpace(FDynamicMesh& Mesh)
	{
		float MinX = FLT_MAX;
		float MinY = FLT_MAX;
		for (FVertex& Vertex : Mesh.Vertices)
		{
			const float ScreenX = Vertex.Position.Y;
			const float ScreenY = -Vertex.Position.Z;
			Vertex.Position = FVector(ScreenX, ScreenY, 0.0f);
			MinX = (std::min)(MinX, Vertex.Position.X);
			MinY = (std::min)(MinY, Vertex.Position.Y);
		}

		for (FVertex& Vertex : Mesh.Vertices)
		{
			Vertex.Position.X -= MinX;
			Vertex.Position.Y -= MinY;
		}
	}

	static bool HasClip(const FUIDrawElement& Element)
	{
		return Element.bHasClipRect && Element.ClipRect.IsValid();
	}

	static FUIRect IntersectUIRect(const FUIRect& A, const FUIRect& B)
	{
		const float X0 = (std::max)(A.X, B.X);
		const float Y0 = (std::max)(A.Y, B.Y);
		const float X1 = (std::min)(A.X + A.Width, B.X + B.Width);
		const float Y1 = (std::min)(A.Y + A.Height, B.Y + B.Height);

		FUIRect Out;
		Out.X = X0;
		Out.Y = Y0;
		Out.Width = (std::max)(0.0f, X1 - X0);
		Out.Height = (std::max)(0.0f, Y1 - Y0);
		return Out;
	}

	static bool ResolveClippedRect(const FUIDrawElement& Element, FUIRect& OutRect)
	{
		OutRect = Element.Rect;
		if (!OutRect.IsValid())
		{
			return false;
		}

		if (!HasClip(Element))
		{
			return true;
		}

		OutRect = IntersectUIRect(OutRect, Element.ClipRect);
		return OutRect.IsValid();
	}

	static FVertex MakeVertex(float X, float Y, float U, float V, const FVector4& Color)
	{
		FVertex Out{};
		Out.Position = FVector(X, Y, 0.0f);
		Out.Color = Color;
		Out.Normal = FVector(0.0f, 0.0f, 1.0f);
		Out.UV = FVector2(U, V);
		return Out;
	}

	static void AppendQuad(
		FDynamicMesh& Mesh,
		float X0, float Y0,
		float X1, float Y1,
		float U0, float V0,
		float U1, float V1,
		const FVector4& Color)
	{
		if (X1 <= X0 || Y1 <= Y0)
		{
			return;
		}

		const uint32 Base = static_cast<uint32>(Mesh.Vertices.size());
		Mesh.Vertices.push_back(MakeVertex(X0, Y0, U0, V0, Color));
		Mesh.Vertices.push_back(MakeVertex(X1, Y0, U1, V0, Color));
		Mesh.Vertices.push_back(MakeVertex(X1, Y1, U1, V1, Color));
		Mesh.Vertices.push_back(MakeVertex(X0, Y1, U0, V1, Color));

		Mesh.Indices.push_back(Base + 0);
		Mesh.Indices.push_back(Base + 1);
		Mesh.Indices.push_back(Base + 2);
		Mesh.Indices.push_back(Base + 0);
		Mesh.Indices.push_back(Base + 2);
		Mesh.Indices.push_back(Base + 3);
	}

	static void AppendClippedQuad(
		FDynamicMesh& Mesh,
		float X0, float Y0,
		float X1, float Y1,
		float U0, float V0,
		float U1, float V1,
		const FVector4& Color,
		const FUIRect* ClipRect)
	{
		if (X1 <= X0 || Y1 <= Y0)
		{
			return;
		}

		float CX0 = X0;
		float CY0 = Y0;
		float CX1 = X1;
		float CY1 = Y1;

		if (ClipRect)
		{
			const float NX0 = (std::max)(X0, ClipRect->X);
			const float NY0 = (std::max)(Y0, ClipRect->Y);
			const float NX1 = (std::min)(X1, ClipRect->X + ClipRect->Width);
			const float NY1 = (std::min)(Y1, ClipRect->Y + ClipRect->Height);
			if (NX1 <= NX0 || NY1 <= NY0)
			{
				return;
			}

			CX0 = NX0;
			CY0 = NY0;
			CX1 = NX1;
			CY1 = NY1;
		}

		const float Width = X1 - X0;
		const float Height = Y1 - Y0;

		const float TU0 = U0 + ((CX0 - X0) / Width) * (U1 - U0);
		const float TU1 = U0 + ((CX1 - X0) / Width) * (U1 - U0);
		const float TV0 = V0 + ((CY0 - Y0) / Height) * (V1 - V0);
		const float TV1 = V0 + ((CY1 - Y0) / Height) * (V1 - V0);

		AppendQuad(Mesh, CX0, CY0, CX1, CY1, TU0, TV0, TU1, TV1, Color);
	}

	static bool MeasureMeshBounds(const FDynamicMesh& Mesh, float& OutMaxX, float& OutMaxY)
	{
		if (Mesh.Vertices.empty())
		{
			OutMaxX = 0.0f;
			OutMaxY = 0.0f;
			return false;
		}

		OutMaxX = 0.0f;
		OutMaxY = 0.0f;
		for (const FVertex& Vertex : Mesh.Vertices)
		{
			OutMaxX = (std::max)(OutMaxX, Vertex.Position.X);
			OutMaxY = (std::max)(OutMaxY, Vertex.Position.Y);
		}
		return true;
	}

	static int32 MakeDepthSortKey(float Depth)
	{
		if (!std::isfinite(Depth))
		{
			return 0;
		}

		constexpr double DepthScale = 1024.0;
		const double Scaled = static_cast<double>(Depth) * DepthScale;
		const double MinKey = static_cast<double>((std::numeric_limits<int32>::min)());
		const double MaxKey = static_cast<double>((std::numeric_limits<int32>::max)());
		const double Clamped = (std::max)(MinKey, (std::min)(Scaled, MaxKey));
		return static_cast<int32>(std::llround(Clamped));
	}
}

bool FScreenUIRenderer::DrawBatchCommand(FRenderer& Renderer, const FUIBatchCommand& BatchCommand)
{
	if (!BatchCommand.Mesh || !BatchCommand.Material)
	{
		return true;
	}

	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!DeviceContext)
	{
		return false;
	}

	if (!BatchCommand.Mesh->UpdateVertexAndIndexBuffer(Renderer.GetDevice(), DeviceContext))
	{
		return false;
	}

	BatchCommand.Material->Bind(DeviceContext);

	FRenderStateManager& RenderStateManager = *Renderer.GetRenderStateManager();
	RenderStateManager.BindState(BatchCommand.Material->GetRasterizerState());
	RenderStateManager.BindState(BatchCommand.Material->GetDepthStencilState());
	RenderStateManager.BindState(BatchCommand.Material->GetBlendState());

	if (!BatchCommand.Material->HasPixelTextureBinding())
	{
		DeviceContext->PSSetSamplers(0, 1, &Renderer.NormalSampler);
	}

	BatchCommand.Mesh->Bind(DeviceContext);
	Renderer.UpdateObjectConstantBuffer(FMatrix::Identity);

	if (!BatchCommand.Mesh->Indices.empty())
	{
		DeviceContext->DrawIndexed(static_cast<UINT>(BatchCommand.Mesh->Indices.size()), 0, 0);
	}
	else
	{
		DeviceContext->Draw(static_cast<UINT>(BatchCommand.Mesh->Vertices.size()), 0);
	}

	return true;
}

FScreenUIRenderer::~FScreenUIRenderer()
{
	ResetFrame();
}

void FScreenUIRenderer::ResetFrame()
{
	UIBatch.Clear();
	FrameMeshes.clear();
}

FDynamicMesh* FScreenUIRenderer::CreateFrameMesh(EMeshTopology Topology)
{
	auto Mesh = std::make_unique<FDynamicMesh>();
	Mesh->Topology = Topology;
	Mesh->bIsDirty = true;

	FDynamicMesh* RawMesh = Mesh.get();
	FrameMeshes.push_back(std::move(Mesh));
	return RawMesh;
}

FDynamicMaterial* FScreenUIRenderer::GetOrCreateColorMaterial(FRenderer& Renderer)
{
	if (UIColorMaterial)
	{
		return UIColorMaterial.get();
	}

	if (!Renderer.GetDefaultMaterial())
	{
		return nullptr;
	}

	UIColorMaterial = Renderer.GetDefaultMaterial()->CreateDynamicMaterial();
	if (!UIColorMaterial)
	{
		return nullptr;
	}

	const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);
	UIColorMaterial->SetVectorParameter("BaseColor", White);

	FDepthStencilStateOption DepthOpt = UIColorMaterial->GetDepthStencilOption();
	DepthOpt.DepthEnable = false;
	DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	auto DSS = Renderer.GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOpt);
	UIColorMaterial->SetDepthStencilOption(DepthOpt);
	UIColorMaterial->SetDepthStencilState(DSS);
	return UIColorMaterial.get();
}

FDynamicMaterial* FScreenUIRenderer::GetOrCreateFontMaterial(FRenderer& Renderer, uint32 Color)
{
	auto It = FontMaterialByColor.find(Color);
	if (It != FontMaterialByColor.end())
	{
		return It->second.get();
	}

	ISceneTextFeature* TextFeature = Renderer.GetSceneTextFeature();
	if (!TextFeature)
	{
		return nullptr;
	}

	FMaterial* FontMaterial = TextFeature->GetBaseMaterial();
	if (!FontMaterial)
	{
		return nullptr;
	}

	auto Material = FontMaterial->CreateDynamicMaterial();
	if (!Material)
	{
		return nullptr;
	}

	Material->SetVectorParameter("TextColor", ToColor(Color));

	FDepthStencilStateOption DepthOpt = Material->GetDepthStencilOption();
	DepthOpt.DepthEnable = false;
	DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	auto DSS = Renderer.GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOpt);
	Material->SetDepthStencilOption(DepthOpt);
	Material->SetDepthStencilState(DSS);

	FDynamicMaterial* Raw = Material.get();
	FontMaterialByColor[Color] = std::move(Material);
	return Raw;
}

void FScreenUIRenderer::EnqueueMesh(FDynamicMesh* Mesh, FMaterial* Material, int32 Layer, float Depth)
{
	if (!Mesh || !Material || Mesh->Vertices.empty())
	{
		return;
	}

	const int32 DepthSortKey = MakeDepthSortKey(Depth);

	if (!UIBatch.Commands.empty())
	{
		FUIBatchCommand& Last = UIBatch.Commands.back();
		if (Last.Mesh && Last.Material == Material
			&& Last.Layer == Layer
			&& Last.DepthSortKey == DepthSortKey
			&& Last.Mesh->Topology == Mesh->Topology
			&& (Last.Mesh->Indices.empty() == Mesh->Indices.empty()))
		{
			const uint32 VertexBase = static_cast<uint32>(Last.Mesh->Vertices.size());
			Last.Mesh->Vertices.insert(Last.Mesh->Vertices.end(), Mesh->Vertices.begin(), Mesh->Vertices.end());
			if (!Mesh->Indices.empty())
			{
				Last.Mesh->Indices.reserve(Last.Mesh->Indices.size() + Mesh->Indices.size());
				for (const uint32 Index : Mesh->Indices)
				{
					Last.Mesh->Indices.push_back(VertexBase + Index);
				}
			}

			Last.Mesh->bIsDirty = true;
			Mesh->Vertices.clear();
			Mesh->Indices.clear();
			Mesh->bIsDirty = true;
			return;
		}
	}

	FUIBatchCommand Command;
	Command.Mesh = Mesh;
	Command.Material = Material;
	Command.Layer = Layer;
	Command.Depth = Depth;
	Command.DepthSortKey = DepthSortKey;
	UIBatch.Commands.push_back(Command);
}

void FScreenUIRenderer::AppendFilledRect(const FUIDrawElement& Element)
{
	FUIRect DrawRect;
	if (!ResolveClippedRect(Element, DrawRect))
	{
		return;
	}

	FDynamicMesh* Mesh = CreateFrameMesh(EMeshTopology::EMT_TriangleList);
	if (!Mesh)
	{
		return;
	}

	const FVector4 Color = ToColor(Element.Color);
	AppendQuad(
		*Mesh,
		DrawRect.X,
		DrawRect.Y,
		DrawRect.X + DrawRect.Width,
		DrawRect.Y + DrawRect.Height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		Color);

	Mesh->bIsDirty = true;
}

void FScreenUIRenderer::AppendRectOutline(const FUIDrawElement& Element)
{
	if (!Element.Rect.IsValid())
	{
		return;
	}

	FDynamicMesh* Mesh = CreateFrameMesh(EMeshTopology::EMT_TriangleList);
	if (!Mesh)
	{
		return;
	}

	const FVector4 Color = ToColor(Element.Color);
	const FUIRect* Clip = HasClip(Element) ? &Element.ClipRect : nullptr;
	const float X = Element.Rect.X;
	const float Y = Element.Rect.Y;
	const float W = Element.Rect.Width;
	const float H = Element.Rect.Height;
	const float T = 1.0f;

	AppendClippedQuad(*Mesh, X, Y, X + W, Y + T, 0, 0, 0, 0, Color, Clip);
	AppendClippedQuad(*Mesh, X, Y + H - T, X + W, Y + H, 0, 0, 0, 0, Color, Clip);
	AppendClippedQuad(*Mesh, X, Y + T, X + T, Y + H - T, 0, 0, 0, 0, Color, Clip);
	AppendClippedQuad(*Mesh, X + W - T, Y + T, X + W, Y + H - T, 0, 0, 0, 0, Color, Clip);

	Mesh->bIsDirty = true;
}

void FScreenUIRenderer::AppendText(FRenderer& Renderer, const FUIDrawElement& Element)
{
	if (Element.Text.empty())
	{
		return;
	}

	FDynamicMaterial* FontMaterial = GetOrCreateFontMaterial(Renderer, Element.Color);
	if (!FontMaterial)
	{
		return;
	}

	FDynamicMesh SourceMesh;
	SourceMesh.Topology = EMeshTopology::EMT_TriangleList;
	ISceneTextFeature* TextFeature = Renderer.GetSceneTextFeature();
	if (!TextFeature || !TextFeature->BuildMesh(Element.Text, SourceMesh, Element.LetterSpacing))
	{
		return;
	}

	ConvertTextMeshToScreenSpace(SourceMesh);

	float MaxX = 0.0f;
	float MaxY = 0.0f;
	MeasureMeshBounds(SourceMesh, MaxX, MaxY);

	if (HasClip(Element))
	{
		FUIRect TextBounds;
		TextBounds.X = Element.Point.X;
		TextBounds.Y = Element.Point.Y;
		TextBounds.Width = MaxX * Element.FontSize;
		TextBounds.Height = MaxY * Element.FontSize;

		const FUIRect Clipped = IntersectUIRect(TextBounds, Element.ClipRect);
		if (!Clipped.IsValid())
		{
			return;
		}
	}

	FDynamicMesh* Mesh = CreateFrameMesh(EMeshTopology::EMT_TriangleList);
	if (!Mesh)
	{
		return;
	}

	const FUIRect* Clip = HasClip(Element) ? &Element.ClipRect : nullptr;
	const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);

	// TextMeshBuilder는 글자당 4 vertices / 6 indices 쿼드를 만든다.
	for (size_t VertexBase = 0; VertexBase + 3 < SourceMesh.Vertices.size(); VertexBase += 4)
	{
		const FVertex& SV0 = SourceMesh.Vertices[VertexBase + 0];
		const FVertex& SV1 = SourceMesh.Vertices[VertexBase + 1];
		const FVertex& SV2 = SourceMesh.Vertices[VertexBase + 2];
		const FVertex& SV3 = SourceMesh.Vertices[VertexBase + 3];

		const float X0 = Element.Point.X + SV0.Position.X * Element.FontSize;
		const float Y0 = Element.Point.Y + SV0.Position.Y * Element.FontSize;
		const float X1 = Element.Point.X + SV2.Position.X * Element.FontSize;
		const float Y1 = Element.Point.Y + SV2.Position.Y * Element.FontSize;

		AppendClippedQuad(
			*Mesh,
			X0,
			Y0,
			X1,
			Y1,
			SV0.UV.X,
			SV0.UV.Y,
			SV2.UV.X,
			SV2.UV.Y,
			White,
			Clip);
	}

	if (Mesh->Vertices.empty())
	{
		return;
	}

	Mesh->bIsDirty = true;
	EnqueueMesh(Mesh, FontMaterial, Element.Layer, Element.Depth);
}

void FScreenUIRenderer::ApplyOrthoProjection(int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		OrthoProjection = FMatrix::Identity;
		return;
	}

	OrthoProjection = FMatrix(
		2.0f / Width, 0, 0, 0,
		0, -2.0f / Height, 0, 0,
		0, 0, 1, 0,
		-1, 1, 0, 1
	);
}

bool FScreenUIRenderer::RenderBatch(FRenderer& Renderer)
{
	if (UIBatch.Commands.empty())
	{
		return true;
	}

	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!DeviceContext || !Renderer.GetRenderStateManager())
	{
		return false;
	}

	Renderer.RenderDevice.BindSwapChainRTV();
	Renderer.ViewMatrix = UIBatch.ViewMatrix;
	Renderer.ProjectionMatrix = UIBatch.ProjectionMatrix;
	Renderer.ShaderManager.Bind(DeviceContext);
	Renderer.SetConstantBuffers();
	Renderer.UpdateFrameConstantBuffer();
	Renderer.GetRenderStateManager()->RebindState();

	for (const FUIBatchCommand& BatchCommand : UIBatch.Commands)
	{
		if (!DrawBatchCommand(Renderer, BatchCommand))
		{
			return false;
		}
	}

	return true;
}

bool FScreenUIRenderer::Render(FRenderer& Renderer, const FUIDrawList& DrawList)
{
	ResetFrame();
	if (DrawList.Elements.empty() || DrawList.ScreenWidth <= 0 || DrawList.ScreenHeight <= 0)
	{
		return true;
	}

	ApplyOrthoProjection(DrawList.ScreenWidth, DrawList.ScreenHeight);
	UIBatch.ViewMatrix = FMatrix::Identity;
	UIBatch.ProjectionMatrix = OrthoProjection;
	UIBatch.Reserve(DrawList.Elements.size());

	FDynamicMaterial* ColorMaterial = GetOrCreateColorMaterial(Renderer);
	if (!ColorMaterial)
	{
		ResetFrame();
		return false;
	}

	TArray<const FUIDrawElement*> SortedElements;
	SortedElements.reserve(DrawList.Elements.size());
	for (const FUIDrawElement& Element : DrawList.Elements)
	{
		SortedElements.push_back(&Element);
	}

	std::stable_sort(
		SortedElements.begin(),
		SortedElements.end(),
		[](const FUIDrawElement* A, const FUIDrawElement* B)
		{
			if (A->Layer != B->Layer)
			{
				return A->Layer < B->Layer;
			}
			const int32 ADepthKey = MakeDepthSortKey(A->Depth);
			const int32 BDepthKey = MakeDepthSortKey(B->Depth);
			if (ADepthKey != BDepthKey)
			{
				return ADepthKey < BDepthKey;
			}
			return A->Order < B->Order;
		});

	for (const FUIDrawElement* ElementPtr : SortedElements)
	{
		if (!ElementPtr)
		{
			continue;
		}

		const FUIDrawElement& Element = *ElementPtr;
		const size_t PrevMeshCount = FrameMeshes.size();

		switch (Element.Type)
		{
		case EUIDrawElementType::FilledRect:
			AppendFilledRect(Element);
			if (FrameMeshes.size() > PrevMeshCount)
			{
				EnqueueMesh(FrameMeshes.back().get(), ColorMaterial, Element.Layer, Element.Depth);
			}
			break;

		case EUIDrawElementType::RectOutline:
			AppendRectOutline(Element);
			if (FrameMeshes.size() > PrevMeshCount)
			{
				EnqueueMesh(FrameMeshes.back().get(), ColorMaterial, Element.Layer, Element.Depth);
			}
			break;

		case EUIDrawElementType::Text:
			AppendText(Renderer, Element);
			break;
		}
	}

	if (UIBatch.Commands.empty())
	{
		ResetFrame();
		return true;
	}

	const bool bRendered = RenderBatch(Renderer);
	ResetFrame();
	return bRendered;
}
