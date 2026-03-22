#include "LineBatcher.h"
#include "Core/EngineTypes.h"
#include "Editor/Settings/EditorSettings.h"

void FLineBatcher::Create(ID3D11Device* InDevice)
{
	Release();

	Device = InDevice;
	if (!Device)
	{
		return;
	}

	Device->AddRef(); // %%% 이게 뭐지?

	MaxVertexCount = 4096; // %%% 다른 곳에서 선언할 수 있는지 찾아보셈

	D3D11_BUFFER_DESC VBDesc = { };
	VBDesc.ByteWidth = sizeof(FLineVertex) * MaxVertexCount;
	VBDesc.Usage = D3D11_USAGE_DYNAMIC;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	/*VBDesc.MiscFlags = 0; %%% 뭔지 찾아보고 쓸모 있으면 쓰셈
	VBDesc.StructureByteStride = sizeof(FLineVertex);*/

	if (FAILED(Device->CreateBuffer(&VBDesc, nullptr, &VertexBuffer)))
{
		Device->Release();
		VertexBuffer = nullptr;
		MaxVertexCount = 0;
		return;
	}
}

void FLineBatcher::Release()
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}

	if (Device)
	{
		Device->Release();
		Device = nullptr;
}

	MaxVertexCount = 0;
	Vertices.clear();
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FVector4& InColor)
{
	Vertices.emplace_back(Start, InColor);
	Vertices.emplace_back(End, InColor);
}

void FLineBatcher::AddAABB(const FBoundingBox& Box, const FColor& InColor)
{
	const FVector4 BoxColor = InColor.ToVector4();

	// @@@ 아래 면
	const FVector v0 = FVector(Box.Min.X, Box.Min.Y, Box.Min.Z);
	const FVector v1 = FVector(Box.Max.X, Box.Min.Y, Box.Min.Z);
	const FVector v2 = FVector(Box.Max.X, Box.Max.Y, Box.Min.Z);
	const FVector v3 = FVector(Box.Min.X, Box.Max.Y, Box.Min.Z);
	// @@@ 윗면
	const FVector v4 = FVector(Box.Min.X, Box.Min.Y, Box.Max.Z);
	const FVector v5 = FVector(Box.Max.X, Box.Min.Y, Box.Max.Z);
	const FVector v6 = FVector(Box.Max.X, Box.Max.Y, Box.Max.Z);
	const FVector v7 = FVector(Box.Min.X, Box.Max.Y, Box.Max.Z);

	// @@@ 아래 면
	AddLine(v0, v1, BoxColor);
	AddLine(v1, v2, BoxColor);
	AddLine(v2, v3, BoxColor);
	AddLine(v3, v0, BoxColor);
	// @@@ 윗면
	AddLine(v4, v5, BoxColor);
	AddLine(v5, v6, BoxColor);
	AddLine(v6, v7, BoxColor);
	AddLine(v7, v4, BoxColor);
	// @@@ 수직 edges
	AddLine(v0, v4, BoxColor);
	AddLine(v1, v5, BoxColor);
	AddLine(v2, v6, BoxColor);
	AddLine(v3, v7, BoxColor);
}

void FLineBatcher::AddWorldGrid(float GridSpacing, int HalfGridCount)
{
	if (GridSpacing <= 0.0f) return;

	FEditorSettings& Settings = FEditorSettings::Get();
	float Spacing = Settings.GridSpacing;         // 그리드 간격
	int32 HalfCount = Settings.GridHalfLineCount; // 반쪽 라인 수

	const FVector4 AxisColor = FColor::White().ToVector4();
	
	// @@@ Z축
	AddLine(
		FVector(0.0f, 0.0f, -HalfCount * Spacing),
		FVector(0.0f, 0.0f, HalfCount * Spacing),
		FColor::Blue().ToVector4()
	);
	
	for (int i = -HalfCount; i <= HalfCount; i++)
	{
		const float Offset = i * Spacing;

		// @@@ X축 평행선
		AddLine(
			FVector(-HalfCount * Spacing, Offset, 0.0f),
			FVector(HalfCount * Spacing, Offset, 0.0f),
			(i == 0) ? FVector4{ 1.0f, 0.0f, 0.0f, 1.0f } : AxisColor
		);

		// @@@ Y축 평행선
		AddLine(
			FVector(Offset, -HalfCount * Spacing, 0.0f),
			FVector(Offset, HalfCount * Spacing, 0.0f),
			(i == 0) ? FVector4{ 0.0f, 1.0f, 0.0f, 1.0f } : AxisColor
		);
	}
}

void FLineBatcher::Clear()
{
	Vertices.clear();
}

// %%% Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj) -> Flush(ID3D11DeviceContext* Context)
void FLineBatcher::Flush(ID3D11DeviceContext* Context)
{
	if (!Context) return;
	if (!Device) return;

	// @@@ 그릴 정점이 없다면 return
	if (Vertices.empty()) return;

	// @@@ 현재 정점 수가 버퍼 용량보다 크면 VB 재할당
	const uint32 RequiredVertexCount = static_cast<uint32>(Vertices.size());

	if(!VertexBuffer || RequiredVertexCount > MaxVertexCount)
	{
		if (VertexBuffer)
		{
			VertexBuffer->Release();
			VertexBuffer = nullptr; 
		}

		MaxVertexCount = RequiredVertexCount * 2;

		D3D11_BUFFER_DESC VBDesc = {};
		VBDesc.ByteWidth = sizeof(FLineVertex) * MaxVertexCount;
		VBDesc.Usage = D3D11_USAGE_DYNAMIC;
		VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(Device->CreateBuffer(&VBDesc, nullptr, &VertexBuffer)))
		{
			/*Device->Release();
			Device = nullptr;*/
			VertexBuffer = nullptr;
			MaxVertexCount = 0;
			return;
		}
	}

	// @@@ Map/memcpy/Unmap
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	if (FAILED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
{
		return;
	}
	memcpy(MappedResource.pData, Vertices.data(), sizeof(FLineVertex) * RequiredVertexCount);
	Context->Unmap(VertexBuffer, 0);
	
	// @@@ VB 바인딩 + Topology -> LineList
	UINT Stride = sizeof(FLineVertex);
	UINT Offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// @@@ Draw
	Context->Draw(RequiredVertexCount, 0);
}

uint32 FLineBatcher::GetLineCount() const
{
	return static_cast<uint32>(Vertices.size() / 2);
}
