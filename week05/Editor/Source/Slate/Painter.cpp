#include "Painter.h"

#include "Renderer/Renderer.h"
#include "Component/TextComponent.h"
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

	auto Mesh = std::make_unique<FDynamicMesh>();
	Mesh->Topology = EMeshTopology::EMT_TriangleList;

	const FVector4 C = ToColor(Color);
	auto V = [&](float X, float Y) {
		FVertex Out{};
		Out.Position = FVector(X, Y, 0.0f);
		Out.Color = C;
		Out.Normal = FVector(0.0f, 0.0f, 1.0f);
		Out.UV = FVector2(0, 0);
		return Out;
		};

	Mesh->Vertices = {
		V((float)InRect.X, (float)InRect.Y),
		V((float)(InRect.X + InRect.Width), (float)InRect.Y),
		V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)),
		V((float)InRect.X, (float)(InRect.Y + InRect.Height))
	};
	Mesh->Indices = { 0, 1, 1, 2, 2, 3 };
	Mesh->bIsDirty = true;

	FDynamicMesh* MeshPtr = Mesh.get();
	FrameMeshes.push_back(std::move(Mesh));
	
	FRenderCommand Commend;
	Commend.RenderMesh = MeshPtr;
	Commend.Material = UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial();
	Commend.WorldMatrix = FMatrix::Identity;
	Commend.RenderLayer = ERenderLayer::UI;
	UIQueue.AddCommand(Commend);
}

void FPainter::DrawRectFilled(FRect InRect, uint32 Color)
{
	if (!Renderer || !InRect.IsValid()) return;

	auto Mesh = std::make_unique<FDynamicMesh>();
	Mesh->Topology = EMeshTopology::EMT_TriangleList;

	const FVector4 C = ToColor(Color);
	auto V = [&](float X, float Y) {
		FVertex Out{};
		Out.Position = FVector(X, Y, 0.0f);
		Out.Color = C;
		Out.Normal = FVector(0.0f, 0.0f, 1.0f);
		Out.UV = FVector2(0, 0);
		return Out;
		};

	Mesh->Vertices = {
		V((float)InRect.X, (float)InRect.Y),
		V((float)(InRect.X + InRect.Width), (float)InRect.Y),
		V((float)(InRect.X + InRect.Width), (float)(InRect.Y + InRect.Height)),
		V((float)InRect.X, (float)(InRect.Y + InRect.Height))
	};
	Mesh->Indices = { 0, 1, 2, 0, 2, 3 };
	Mesh->bIsDirty = true;

	FDynamicMesh* MeshPtr = Mesh.get();
	FrameMeshes.push_back(std::move(Mesh));

	FRenderCommand Commend;
	Commend.RenderMesh = MeshPtr;
	Commend.Material = UiColorMaterial ? static_cast<FMaterial*>(UiColorMaterial.get()) : Renderer->GetDefaultMaterial();
	Commend.WorldMatrix = FMatrix::Identity;
	Commend.RenderLayer = ERenderLayer::UI;
	UIQueue.AddCommand(Commend);
}

void FPainter::DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, FDynamicMesh*& InOutMesh)
{
	if (!Renderer || !Text || Text[0] == '\0') return;

	if (!InOutMesh)
	{
		InOutMesh = new FDynamicMesh();
		InOutMesh->Topology = EMeshTopology::EMT_TriangleList;
		if (!Renderer->GetTextRenderer().BuildTextMesh(Text, *InOutMesh))
			return;

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

	FDynamicMaterial* FontMat = nullptr;
	auto It = FontMaterialByColor.find(Color);
	if (It == FontMaterialByColor.end())
	{
		auto M = Renderer->GetTextRenderer().GetFontMaterial()->CreateDynamicMaterial();
		if (!M) return;
		const FVector4 C = ToColor(Color);
		M->SetVectorParameter("TextColor", C);
		FontMat = M.get();
		FontMaterialByColor[Color] = std::move(M);
	}
	else
	{
		FontMat = It->second.get();
	}

	FRenderCommand Command;
	Command.RenderMesh = InOutMesh;
	Command.Material = FontMat;
	Command.WorldMatrix = FMatrix::MakeWorld({ (float)Point.X, (float)Point.Y, 0 }, FMatrix::Identity, FVector::One() * FontSize);
	Command.RenderLayer = ERenderLayer::UI;
	UIQueue.AddCommand(Command);
}

void FPainter::Flush()
{
	if (!Renderer) return;
	UIQueue.ViewMatrix = FMatrix::Identity;
	UIQueue.ProjectionMatrix = OrthoProj;
	Renderer->SubmitCommands(UIQueue);
	Renderer->ExecuteCommands();
	UIQueue.Clear();
}
