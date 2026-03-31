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

	static FDynamicMesh* EnsureBatchMesh(std::unique_ptr<FDynamicMesh>& InOutMesh, EMeshTopology Topology)
	{
		if (!InOutMesh)
		{
			InOutMesh = std::make_unique<FDynamicMesh>();
			InOutMesh->Topology = Topology;
			InOutMesh->bIsDirty = true;
		}
		return InOutMesh.get();
	}

	static void ResetBatchMesh(FDynamicMesh* Mesh)
	{
		if (!Mesh)
		{
			return;
		}

		Mesh->Vertices.clear();
		Mesh->Indices.clear();
		Mesh->bIsDirty = true;
	}
}

FPainter::FPainter(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	if (Renderer && Renderer->GetDefaultMaterial())
	{
		UiColorMaterial = Renderer->GetDefaultMaterial()->CreateDynamicMaterial();
		if (UiColorMaterial)
		{
			FDepthStencilStateOption DepthOpt = UiColorMaterial->GetDepthStencilOption();
			DepthOpt.DepthEnable = false;
			DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			auto DSS = Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOpt);
			UiColorMaterial->SetDepthStencilOption(DepthOpt);
			UiColorMaterial->SetDepthStencilState(DSS);
		}
	}
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

void FPainter::DrawRect(FRect InRect, uint32 Color)
{
	if (!Renderer || !InRect.IsValid()) return;

	FDynamicMesh* Batch = EnsureBatchMesh(UiLineBatchMesh, EMeshTopology::EMT_LineList);
	if (!Batch) return;

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

	const uint32 Base = static_cast<uint32>(Batch->Vertices.size());

	Batch->Vertices.push_back(V((float)InRect.X, (float)InRect.Y));
	Batch->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)InRect.Y));
	Batch->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)));
	Batch->Vertices.push_back(V((float)InRect.X, (float)(InRect.Y + InRect.Height)));

	Batch->Indices.push_back(Base + 0);
	Batch->Indices.push_back(Base + 1);

	Batch->Indices.push_back(Base + 1);
	Batch->Indices.push_back(Base + 2);

	Batch->Indices.push_back(Base + 2);
	Batch->Indices.push_back(Base + 3);

	Batch->Indices.push_back(Base + 3);
	Batch->Indices.push_back(Base + 0);

	Batch->bIsDirty = true;
}

void FPainter::DrawRectFilled(FRect InRect, uint32 Color)
{
	if (!Renderer || !InRect.IsValid()) return;

	FDynamicMesh* Batch = EnsureBatchMesh(UiFilledBatchMesh, EMeshTopology::EMT_TriangleList);
	if (!Batch) return;

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

	const uint32 Base = static_cast<uint32>(Batch->Vertices.size());

	Batch->Vertices.push_back(V((float)InRect.X, (float)InRect.Y));
	Batch->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)InRect.Y));
	Batch->Vertices.push_back(V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)));
	Batch->Vertices.push_back(V((float)InRect.X, (float)(InRect.Y + InRect.Height)));

	Batch->Indices.push_back(Base + 0);
	Batch->Indices.push_back(Base + 1);
	Batch->Indices.push_back(Base + 2);

	Batch->Indices.push_back(Base + 0);
	Batch->Indices.push_back(Base + 2);
	Batch->Indices.push_back(Base + 3);

	Batch->bIsDirty = true;
}

void FPainter::DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, float LetterSpacing, FDynamicMesh*& InOutMesh)
{
	if (!EnsureUiTextMesh(Renderer, Text, LetterSpacing, InOutMesh)) return;

	FDynamicMaterial* FontMat = nullptr;
	auto MatIt = FontMaterialByColor.find(Color);
	if (MatIt == FontMaterialByColor.end())
	{
		auto M = Renderer->GetTextRenderer().GetFontMaterial()->CreateDynamicMaterial();
		if (!M)
		{
			return;
		}

		const FVector4 C = ToColor(Color);
		M->SetVectorParameter("TextColor", C);

		FDepthStencilStateOption DepthOpt = M->GetDepthStencilOption();
		DepthOpt.DepthEnable = false;
		DepthOpt.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		auto DSS = Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOpt);
		M->SetDepthStencilOption(DepthOpt);
		M->SetDepthStencilState(DSS);

		FontMat = M.get();
		FontMaterialByColor[Color] = std::move(M);
	}
	else
	{
		FontMat = MatIt->second.get();
	}

	if (!FontMat) return;

	auto MeshIt = TextBatchMeshByColor.find(Color);
	if (MeshIt == TextBatchMeshByColor.end())
	{
		auto NewBatch = std::make_unique<FDynamicMesh>();
		NewBatch->Topology = InOutMesh->Topology;
		NewBatch->bIsDirty = true;
		MeshIt = TextBatchMeshByColor.emplace(Color, std::move(NewBatch)).first;
	}

	FDynamicMesh* Batch = MeshIt->second.get();
	if (!Batch) return;

	const uint32 Base = static_cast<uint32>(Batch->Vertices.size());

	for (const FVertex& Src : InOutMesh->Vertices)
	{
		FVertex Dst = Src;
		Dst.Position = FVector(
			(float)Point.X + Src.Position.X * FontSize,
			(float)Point.Y + Src.Position.Y * FontSize,
			0.0f
		);
		Batch->Vertices.push_back(Dst);
	}

	for (const auto& Idx : InOutMesh->Indices)
	{
		Batch->Indices.push_back(Base + static_cast<uint32>(Idx));
	}

	Batch->bIsDirty = true;
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

	if (UiLineBatchMesh && !UiLineBatchMesh->Vertices.empty() && !UiLineBatchMesh->Indices.empty())
	{
		FRenderCommand Command;
		Command.RenderMesh = UiLineBatchMesh.get();
		Command.Material = UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial();
		Command.WorldMatrix = FMatrix::Identity;
		Command.RenderLayer = ERenderLayer::UI;
		UIQueue.AddCommand(Command);
	}

	if (UiFilledBatchMesh && !UiFilledBatchMesh->Vertices.empty() && !UiFilledBatchMesh->Indices.empty())
	{
		FRenderCommand Command;
		Command.RenderMesh = UiFilledBatchMesh.get();
		Command.Material = UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial();
		Command.WorldMatrix = FMatrix::Identity;
		Command.RenderLayer = ERenderLayer::UI;
		UIQueue.AddCommand(Command);
	}

	for (auto& Pair : TextBatchMeshByColor)
	{
		const uint32 Color = Pair.first;
		std::unique_ptr<FDynamicMesh>& MeshPtr = Pair.second;

		if (!MeshPtr || MeshPtr->Vertices.empty() || MeshPtr->Indices.empty())
		{
			continue;
		}

		auto MatIt = FontMaterialByColor.find(Color);
		if (MatIt == FontMaterialByColor.end() || !MatIt->second)
		{
			continue;
		}

		FRenderCommand Command;
		Command.RenderMesh = MeshPtr.get();
		Command.Material = MatIt->second.get();
		Command.WorldMatrix = FMatrix::Identity;
		Command.RenderLayer = ERenderLayer::UI;
		UIQueue.AddCommand(Command);
	}

	Renderer->SubmitCommands(UIQueue);
	Renderer->ExecuteCommands();
	UIQueue.Clear();

	::ResetBatchMesh(UiLineBatchMesh.get());
	::ResetBatchMesh(UiFilledBatchMesh.get());

	for (auto& Pair : TextBatchMeshByColor)
	{
		::ResetBatchMesh(Pair.second.get());
	}
}