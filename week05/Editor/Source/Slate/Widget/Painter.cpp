#include "Painter.h"

#include "Renderer/Renderer.h"
#include "Component/TextComponent.h"
#include <limits>
#include <algorithm>

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

	static bool EnsureUiTextMesh(FRenderer* Renderer, const char* Text, float LetterSpacing, FDynamicMesh*& InOutMesh)
	{
		if (!Renderer || !Text || Text[0] == '\0')
		{
			return false;
		}

		if (!InOutMesh)
		{
			InOutMesh = new FDynamicMesh();
			InOutMesh->Topology = EMeshTopology::EMT_TriangleList;
			if (!Renderer->GetTextRenderer().BuildTextMesh(Text, *InOutMesh, LetterSpacing))
			{
				delete InOutMesh;
				InOutMesh = nullptr;
				return false;
			}

			float MinX = (std::numeric_limits<float>::max)();
			float MinY = (std::numeric_limits<float>::max)();

			for (FVertex& Vertex : InOutMesh->Vertices)
			{
				const float ScreenX = Vertex.Position.Y;
				const float ScreenY = -Vertex.Position.Z;
				Vertex.Position = FVector(ScreenX, ScreenY, 0.0f);
				MinX = (std::min)(MinX, Vertex.Position.X);
				MinY = (std::min)(MinY, Vertex.Position.Y);
			}

			for (FVertex& Vertex : InOutMesh->Vertices)
			{
				Vertex.Position.X -= MinX;
				Vertex.Position.Y -= MinY;
			}

			InOutMesh->bIsDirty = true;
		}

		return true;
	}
}

FPainter::FPainter(FRenderer* InRenderer)
	: Renderer(nullptr)
{
	SetRenderer(InRenderer);
}

void FPainter::SetRenderer(FRenderer* InRenderer)
{
	if (Renderer == InRenderer)
	{
		return;
	}

	Renderer = InRenderer;
	UiColorMaterial.reset();
	FontMaterialByColor.clear();
	FrameMeshes.clear();
	ActiveFrameMeshCount = 0;
	UIQueue.Clear();

	if (!Renderer || !Renderer->GetDefaultMaterial())
	{
		return;
	}

	UiColorMaterial = Renderer->GetDefaultMaterial()->CreateDynamicMaterial();
}

void FPainter::SetScreenSize(int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		OrthoProj = FMatrix::Identity;
		return;
	}

	OrthoProj = FMatrix(
		2.0f / Width, 0, 0, 0,
		0, -2.0f / Height, 0, 0,
		0, 0, 1, 0,
		-1, 1, 0, 1
	);
}

FDynamicMesh* FPainter::CreateFrameMesh(EMeshTopology Topology)
{
	if (ActiveFrameMeshCount >= FrameMeshes.size())
	{
		FrameMeshes.push_back(std::make_unique<FDynamicMesh>());
	}

	FDynamicMesh* Mesh = FrameMeshes[ActiveFrameMeshCount++].get();
	Mesh->Topology = Topology;
	Mesh->Vertices.clear();
	Mesh->Indices.clear();
	Mesh->Sections.clear();
	Mesh->bIsDirty = true;
	return Mesh;
}

FDynamicMaterial* FPainter::GetOrCreateFontMaterial(uint32 Color)
{
	if (!Renderer)
	{
		return nullptr;
	}

	auto MatIt = FontMaterialByColor.find(Color);
	if (MatIt != FontMaterialByColor.end())
	{
		return MatIt->second.get();
	}

	auto Material = Renderer->GetTextRenderer().GetFontMaterial()->CreateDynamicMaterial();
	if (!Material)
	{
		return nullptr;
	}

	const FVector4 C = ToColor(Color);
	Material->SetVectorParameter("TextColor", C);

	FDynamicMaterial* RawMaterial = Material.get();
	FontMaterialByColor[Color] = std::move(Material);
	return RawMaterial;
}

void FPainter::EnqueueMesh(FDynamicMesh* Mesh, FMaterial* Material)
{
	if (!Mesh || !Material || Mesh->Vertices.empty())
	{
		return;
	}

	FRenderCommand Command;
	Command.RenderMesh = Mesh;
	Command.Material = Material;
	Command.WorldMatrix = FMatrix::Identity;
	Command.RenderPass = ERenderPass::UI;
	Command.bOverrideRenderPass = true;
	UIQueue.AddCommand(Command);
}

void FPainter::DrawRect(FRect InRect, uint32 Color)
{
	if (!Renderer || !InRect.IsValid()) return;

	FDynamicMesh* Mesh = CreateFrameMesh(EMeshTopology::EMT_LineList);
	if (!Mesh) return;

	const FVector4 C = ToColor(Color);
	auto V = [&](float X, float Y)
		{
			FVertex Out{};
			Out.Position = FVector(X, Y, 0.0f);
			Out.Color = C;
			Out.Normal = FVector(0.0f, 0.0f, 1.0f);
			Out.UV = FVector2(0, 0);
			return Out;
		};

	const uint32 Base = static_cast<uint32>(Mesh->Vertices.size());

	Mesh->Vertices.push_back(V((float)InRect.X, (float)InRect.Y));
	Mesh->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)InRect.Y));
	Mesh->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)));
	Mesh->Vertices.push_back(V((float)InRect.X, (float)(InRect.Y + InRect.Height)));

	Mesh->Indices.push_back(Base + 0);
	Mesh->Indices.push_back(Base + 1);

	Mesh->Indices.push_back(Base + 1);
	Mesh->Indices.push_back(Base + 2);

	Mesh->Indices.push_back(Base + 2);
	Mesh->Indices.push_back(Base + 3);

	Mesh->Indices.push_back(Base + 3);
	Mesh->Indices.push_back(Base + 0);

	Mesh->bIsDirty = true;
	EnqueueMesh(Mesh, UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial());
}

void FPainter::DrawRectFilled(FRect InRect, uint32 Color)
{
	if (!Renderer || !InRect.IsValid()) return;

	FDynamicMesh* Mesh = CreateFrameMesh(EMeshTopology::EMT_TriangleList);
	if (!Mesh) return;

	const FVector4 C = ToColor(Color);
	auto V = [&](float X, float Y)
		{
			FVertex Out{};
			Out.Position = FVector(X, Y, 0.0f);
			Out.Color = C;
			Out.Normal = FVector(0.0f, 0.0f, 1.0f);
			Out.UV = FVector2(0, 0);
			return Out;
		};

	const uint32 Base = static_cast<uint32>(Mesh->Vertices.size());

	Mesh->Vertices.push_back(V((float)InRect.X, (float)InRect.Y));
	Mesh->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)InRect.Y));
	Mesh->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)));
	Mesh->Vertices.push_back(V((float)InRect.X, (float)(InRect.Y + InRect.Height)));

	Mesh->Indices.push_back(Base + 0);
	Mesh->Indices.push_back(Base + 1);
	Mesh->Indices.push_back(Base + 2);

	Mesh->Indices.push_back(Base + 0);
	Mesh->Indices.push_back(Base + 2);
	Mesh->Indices.push_back(Base + 3);

	Mesh->bIsDirty = true;
	EnqueueMesh(Mesh, UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial());
}

void FPainter::DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, float LetterSpacing, FDynamicMesh*& InOutMesh)
{
	if (!EnsureUiTextMesh(Renderer, Text, LetterSpacing, InOutMesh)) return;

	FDynamicMaterial* FontMat = GetOrCreateFontMaterial(Color);
	if (!FontMat) return;

	FDynamicMesh* Mesh = CreateFrameMesh(InOutMesh->Topology);
	if (!Mesh) return;

	for (const FVertex& Src : InOutMesh->Vertices)
	{
		FVertex Dst = Src;
		Dst.Position = FVector(
			(float)Point.X + Src.Position.X * FontSize,
			(float)Point.Y + Src.Position.Y * FontSize,
			0.0f
		);
		Mesh->Vertices.push_back(Dst);
	}

	for (const auto& Idx : InOutMesh->Indices)
	{
		Mesh->Indices.push_back(static_cast<uint32>(Idx));
	}

	Mesh->bIsDirty = true;
	EnqueueMesh(Mesh, FontMat);
}

FVector2 FPainter::MeasureText(const char* Text, float FontSize, float LetterSpacing, FDynamicMesh*& InOutMesh)
{
	if (!EnsureUiTextMesh(Renderer, Text, LetterSpacing, InOutMesh)) return { 0.0f, 0.0f };

	float MaxX = 0.0f;
	float MaxY = 0.0f;
	for (const FVertex& V : InOutMesh->Vertices)
	{
		MaxX = (std::max)(MaxX, V.Position.X);
		MaxY = (std::max)(MaxY, V.Position.Y);
	}

	return { MaxX * FontSize, MaxY * FontSize };
}

void FPainter::Flush()
{
	if (!Renderer) return;
	UIQueue.ViewMatrix = FMatrix::Identity;
	UIQueue.ProjectionMatrix = OrthoProj;

	if (!UIQueue.Commands.empty())
	{
		Renderer->SubmitCommands(UIQueue);
		Renderer->ExecuteCommands();
	}
	UIQueue.Clear();
	ActiveFrameMeshCount = 0;
}
