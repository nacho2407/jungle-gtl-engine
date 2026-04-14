#include <d3d11.h>
#include <UI/EditorConsoleWidget.h>
#include "SubUVBatcher.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceManager.h"

void FSubUVBatcher::Create(ID3D11Device* InDevice)
{
    Device = InDevice;

    MaxVertexCount = 256;
    MaxIndexCount  = 384;
    CreateBuffers();

	UMaterial* SubUVMaterial = FResourceManager::Get().GetMaterial("SubUVMat");
	SubUVMaterial->DepthStencilType = EDepthStencilType::Default;
	SubUVMaterial->BlendType = EBlendType::AlphaBlend;
	SubUVMaterial->RasterizerType = ERasterizerType::SolidBackCull;
	SubUVMaterial->SamplerType = ESamplerType::EST_Linear;

	Material = SubUVMaterial;
}

void FSubUVBatcher::CreateBuffers()
{
    VertexBuffer.Reset();
    IndexBuffer.Reset();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth      = sizeof(FTextureVertex) * MaxVertexCount;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&vbDesc, nullptr, VertexBuffer.ReleaseAndGetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage          = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth      = sizeof(uint32) * MaxIndexCount;
    ibDesc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&ibDesc, nullptr, IndexBuffer.ReleaseAndGetAddressOf());
}

void FSubUVBatcher::Release()
{
    Clear();

    VertexBuffer.Reset();
    IndexBuffer.Reset();
	Device.Reset();
}

void FSubUVBatcher::AddSprite(UTexture* Texture, 
							  const FVector& WorldPos,
                              const FVector& CamRight,
                              const FVector& CamUp,
							  const FVector& WorldScale,
                              uint32 FrameIndex,
                              uint32 Columns,
                              uint32 Rows,
                              float Width,
                              float Height)
{
	// Batch�� ����ְų� SRV�� 
	if (Batches.empty() || Batches.back().Texture != Texture)
	{
		FSRVBatch batch;
		batch.Texture = Texture;
		batch.IndexStart = static_cast<uint32>(Indices.size());
		batch.IndexCount = 0;
		batch.BaseVertex = static_cast<int32>(Vertices.size());
		Batches.push_back(batch);
	}

    FSubUVFrameInfo Frame = GetFrameUV(FrameIndex, Columns, Rows);

    const float HalfW = Width  * WorldScale.Y * 0.25f;
    const float HalfH = Height * WorldScale.Z * 0.25f;

    FVector v0 = WorldPos + CamRight * (-HalfW) + CamUp * ( HalfH); // �»�
    FVector v1 = WorldPos + CamRight * ( HalfW) + CamUp * ( HalfH); // ���
    FVector v2 = WorldPos + CamRight * (-HalfW) + CamUp * (-HalfH); // ����
    FVector v3 = WorldPos + CamRight * ( HalfW) + CamUp * (-HalfH); // ����

	uint32 LocalBase = static_cast<uint32>(Vertices.size()) 
		- static_cast<uint32>(Batches.back().BaseVertex);

    Vertices.push_back({ v0, { Frame.U,               Frame.V                } });
    Vertices.push_back({ v1, { Frame.U + Frame.Width,  Frame.V                } });
    Vertices.push_back({ v2, { Frame.U,               Frame.V + Frame.Height } });
    Vertices.push_back({ v3, { Frame.U + Frame.Width,  Frame.V + Frame.Height } });

    Indices.push_back(LocalBase + 0); Indices.push_back(LocalBase + 1); Indices.push_back(LocalBase + 2);
	Indices.push_back(LocalBase + 1); Indices.push_back(LocalBase + 3); Indices.push_back(LocalBase + 2);

	Batches.back().IndexCount += 6;
}

void FSubUVBatcher::Clear()
{
    Vertices.clear();
    Indices.clear();
	Batches.clear();
}

void FSubUVBatcher::Flush(ID3D11DeviceContext* Context)
{
    if (Vertices.empty() || !VertexBuffer || !IndexBuffer) return;

    if (Vertices.size() > MaxVertexCount || Indices.size() > MaxIndexCount)
    {
        MaxVertexCount = static_cast<uint32>(Vertices.size()) * 2;
        MaxIndexCount  = static_cast<uint32>(Indices.size())  * 2;
        CreateBuffers();
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(Context->Map(VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, Vertices.data(), sizeof(FTextureVertex) * Vertices.size());
    Context->Unmap(VertexBuffer.Get(), 0);

    if (FAILED(Context->Map(IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, Indices.data(), sizeof(uint32) * Indices.size());
    Context->Unmap(IndexBuffer.Get(), 0);

    uint32 stride = sizeof(FTextureVertex), offset = 0;
	ID3D11Buffer* VertexBufferPtr = VertexBuffer.Get();
	ID3D11Buffer* IndexBufferPtr = IndexBuffer.Get();
    Context->IASetVertexBuffers(0, 1, &VertexBufferPtr, &stride, &offset);
    Context->IASetIndexBuffer(IndexBufferPtr, DXGI_FORMAT_R32_UINT, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UMaterial* Mat = Cast<UMaterial>(Material);

    // Context->PSSetShaderResources(0, 1, &SRV);
	for (const FSRVBatch& Batch : Batches)
	{
		if (!Batch.Texture || Batch.IndexCount == 0) continue;

		Mat->SetTexture("SubUVAtlas", Batch.Texture);
		Material->Bind(Context);

		Context->DrawIndexed(
			Batch.IndexCount,
			Batch.IndexStart,
			Batch.BaseVertex
		);
	}
}

FSubUVFrameInfo FSubUVBatcher::GetFrameUV(uint32 FrameIndex, uint32 Columns, uint32 Rows) const
{
    const float FrameW = 1.0f / static_cast<float>(Columns);
    const float FrameH = 1.0f / static_cast<float>(Rows);

    const uint32 Col = FrameIndex % Columns;
    const uint32 Row = FrameIndex / Columns;

    return { Col * FrameW, Row * FrameH, FrameW, FrameH };
}

