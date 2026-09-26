#include "RenderMesh.h"
#include "Vertex.h"

void FRenderMesh::Bind(ID3D11DeviceContext* Context)
{
	if (!VertexBuffer) return;
	UINT Stride = sizeof(FVertex);
	UINT Offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	if (IndexBuffer)
	{
		Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}
	else
	{
		Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	}
}

void FRenderMesh::Release()
{
	if (IndexBuffer) { IndexBuffer->Release(); IndexBuffer = nullptr; }
	if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
}

void FRenderMesh::UpdateLocalBound()
{
	if (Vertices.empty())
	{
		MinCoord = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		MaxCoord = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		LocalBoundRadius = 0.f;
	}
	else
	{
		// TODO: Ritter's Algorithm으로 개선
		for (const FVertex& Vertex : Vertices)
		{
			if (Vertex.Position.X < MinCoord.X) MinCoord.X = Vertex.Position.X;
			if (Vertex.Position.X > MaxCoord.X) MaxCoord.X = Vertex.Position.X;
			if (Vertex.Position.Y < MinCoord.Y) MinCoord.Y = Vertex.Position.Y;
			if (Vertex.Position.Y > MaxCoord.Y) MaxCoord.Y = Vertex.Position.Y;
			if (Vertex.Position.Z < MinCoord.Z) MinCoord.Z = Vertex.Position.Z;
			if (Vertex.Position.Z > MaxCoord.Z) MaxCoord.Z = Vertex.Position.Z;

			LocalBoundRadius = ((MaxCoord - MinCoord) * 0.5).Size();
		}
	}
}
