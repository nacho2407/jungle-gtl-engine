#include <d3d11.h>
#include <UI/EditorConsoleWidget.h>
#include "SubUVBatcher.h"
#include "Core/CoreTypes.h"

void FSubUVBatcher::Create(ID3D11Device* InDevice)
{
    Device = InDevice;

    MaxVertexCount = 256;
    MaxIndexCount  = 384;
    CreateBuffers();

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    Device->CreateSamplerState(&sampDesc, SamplerState.ReleaseAndGetAddressOf());

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SubUVShader.Create(Device.Get(), L"Shaders/ShaderSubUV.hlsl",
        "VS", "PS", layout, ARRAYSIZE(layout));
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
    SamplerState.Reset();
	Device.Reset();

    SubUVShader.Release();
}

void FSubUVBatcher::AddSprite(ID3D11ShaderResourceView* SRV, 
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
	if (Batches.empty() || Batches.back().SRV != SRV)
	{
		FSRVBatch batch;
		batch.SRV = SRV;
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
    //if (!SRV) return;
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

    SubUVShader.Bind(Context);

    uint32 stride = sizeof(FTextureVertex), offset = 0;
	ID3D11Buffer* VertexBufferPtr = VertexBuffer.Get();
	ID3D11Buffer* IndexBufferPtr = IndexBuffer.Get();
    Context->IASetVertexBuffers(0, 1, &VertexBufferPtr, &stride, &offset);
    Context->IASetIndexBuffer(IndexBufferPtr, DXGI_FORMAT_R32_UINT, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11SamplerState* Samplers[] = { SamplerState.Get() };
	Context->PSSetSamplers(0, 1, Samplers);

    // Context->PSSetShaderResources(0, 1, &SRV);
	for (const FSRVBatch& Batch : Batches)
	{
		if (!Batch.SRV || Batch.IndexCount == 0) continue;

		Context->PSSetShaderResources(0, 1, &Batch.SRV);

	
		Context->DrawIndexed(
			Batch.IndexCount,
			Batch.IndexStart,
			Batch.BaseVertex
		);
	}

    /*Context->DrawIndexed(static_cast<uint32>(Indices.size()), 0, 0);*/
}

FSubUVFrameInfo FSubUVBatcher::GetFrameUV(uint32 FrameIndex, uint32 Columns, uint32 Rows) const
{
    const float FrameW = 1.0f / static_cast<float>(Columns);
    const float FrameH = 1.0f / static_cast<float>(Rows);

    const uint32 Col = FrameIndex % Columns;
    const uint32 Row = FrameIndex / Columns;

    return { Col * FrameW, Row * FrameH, FrameW, FrameH };
}

