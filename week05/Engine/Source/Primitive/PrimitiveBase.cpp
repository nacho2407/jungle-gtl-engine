#include "PrimitiveBase.h"
#include <fstream>
#include <cstdio>

// ─── FMeshData ───

bool FMeshData::UpdateVertexAndIndexBuffer(ID3D11Device* Device)
{
	if (!bIsDirty)
		return true;

	return CreateVertexAndIndexBuffer(Device);
}

bool FMeshData::CreateVertexAndIndexBuffer(ID3D11Device* Device)
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}
	if (IndexBuffer)
	{
		IndexBuffer->Release();
		IndexBuffer = nullptr;
	}

	if (Vertices.empty() || Indices.empty())
	{
		return false;
	}

	// Vertex Buffer
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FPrimitiveVertex) * Vertices.size());
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = Vertices.data();

	HRESULT Hr = Device->CreateBuffer(&VBDesc, &VBData, &VertexBuffer);
	if (FAILED(Hr))
	{
		printf("[FMeshData] Failed to create vertex buffer\n");
		return false;
	}

	// Index Buffer
	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices.data();

	Hr = Device->CreateBuffer(&IBDesc, &IBData, &IndexBuffer);
	if (FAILED(Hr))
	{
		printf("[FMeshData] Failed to create index buffer\n");
		VertexBuffer->Release();
		VertexBuffer = nullptr;
		return false;
	}

	return true;
}

void FMeshData::Bind(ID3D11DeviceContext* Context)
{
	UINT Stride = sizeof(FPrimitiveVertex);
	UINT Offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void FMeshData::Release()
{
	if (IndexBuffer)
	{
		IndexBuffer->Release();
		IndexBuffer = nullptr;
	}
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}
}

void FMeshData::UpdateLocalBound()
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
		for (const FPrimitiveVertex& Vertex : Vertices)
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

// ─── CPrimitiveBase ───

std::unordered_map<FString, std::shared_ptr<FMeshData>> CPrimitiveBase::MeshCache;

std::shared_ptr<FMeshData> CPrimitiveBase::LoadFromFile(const FString& Key, const FString& FilePath)
{
	auto It = MeshCache.find(Key);
	if (It != MeshCache.end())
	{
		return It->second;
	}

	auto Data = LoadFromFile(FilePath);
	if (Data)
	{
		MeshCache[Key] = Data;
		RegisterMeshData(Key, Data);
	}
	return Data;
}

std::shared_ptr<FMeshData> CPrimitiveBase::GetCached(const FString& Key)
{
	auto It = MeshCache.find(Key);
	if (It != MeshCache.end())
	{
		return It->second;
	}
	return nullptr;
}

void CPrimitiveBase::RegisterMeshData(const FString& Key, std::shared_ptr<FMeshData> Data)
{
	Data->UpdateLocalBound();
	MeshCache[Key] = Data;
}

void CPrimitiveBase::ClearCache()
{
	MeshCache.clear();
}

// 바이너리 파일 포맷:
// [uint32] VertexCount
// [FPrimitiveVertex * VertexCount] Vertices
// [uint32] IndexCount
// [uint32 * IndexCount] Indices
std::shared_ptr<FMeshData> CPrimitiveBase::LoadFromFile(const FString& FilePath)
{
	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		printf("[PrimitiveBase] Failed to open mesh file: %s\n", FilePath.c_str());
		return nullptr;
	}

	auto Data = std::make_shared<FMeshData>();

	uint32_t VertexCount = 0;
	File.read(reinterpret_cast<char*>(&VertexCount), sizeof(uint32_t));
	Data->Vertices.resize(VertexCount);
	File.read(reinterpret_cast<char*>(Data->Vertices.data()), VertexCount * sizeof(FPrimitiveVertex));

	uint32_t IndexCount = 0;
	File.read(reinterpret_cast<char*>(&IndexCount), sizeof(uint32_t));
	Data->Indices.resize(IndexCount);
	File.read(reinterpret_cast<char*>(Data->Indices.data()), IndexCount * sizeof(uint32_t));

	// 메시 읽어올 때 기본 옵션 Triangle list
	Data->Topology = EMeshTopology::EMT_TriangleList;

	if (File.fail())
	{
		printf("[PrimitiveBase] Failed to read mesh file: %s\n", FilePath.c_str());
		return nullptr;
	}

	printf("[PrimitiveBase] Loaded mesh: %s (Vertices: %u, Indices: %u)\n",
		FilePath.c_str(), VertexCount, IndexCount);

	return Data;
}
