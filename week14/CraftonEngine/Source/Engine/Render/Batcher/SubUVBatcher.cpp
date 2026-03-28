#include "SubUVBatcher.h"

#include "Core/CoreTypes.h"
#include <UI/EditorConsoleWidget.h>

void FSubUVBatcher::Create(ID3D11Device* InDevice)
{
    Device = InDevice;

    // Dynamic VB/IB 초기 할당 (텍스처는 ResourceManager가 소유 — 여기서 로드하지 않음)
    MaxVertexCount = 256;
    MaxIndexCount  = 384;
    CreateBuffers();

    // Sampler — Linear 필터 (스프라이트는 부드럽게)
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    Device->CreateSamplerState(&sampDesc, &SamplerState);

    // 셰이더 + Input Layout (FTextureVertex: POSITION float3, TEXCOORD float2)
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SubUVShader.Create(Device, L"Shaders/ShaderSubUV.hlsl",
        "VS", "PS", layout, ARRAYSIZE(layout));
}

void FSubUVBatcher::CreateBuffers()
{
    if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
    if (IndexBuffer)  { IndexBuffer->Release();  IndexBuffer  = nullptr; }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth      = sizeof(FTextureVertex) * MaxVertexCount;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&vbDesc, nullptr, &VertexBuffer);

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage          = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth      = sizeof(uint32) * MaxIndexCount;
    ibDesc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&ibDesc, nullptr, &IndexBuffer);
}

void FSubUVBatcher::Release()
{
    Clear();

    if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
    if (IndexBuffer)  { IndexBuffer->Release();  IndexBuffer  = nullptr; }
    if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }

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
	// Batch가 비어있거나 SRV가 
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

    FVector v0 = WorldPos + CamRight * (-HalfW) + CamUp * ( HalfH); // 좌상
    FVector v1 = WorldPos + CamRight * ( HalfW) + CamUp * ( HalfH); // 우상
    FVector v2 = WorldPos + CamRight * (-HalfW) + CamUp * (-HalfH); // 좌하
    FVector v3 = WorldPos + CamRight * ( HalfW) + CamUp * (-HalfH); // 우하

	// ── 핵심: 배치 내 로컬 인덱스를 사용 ──
	// BaseVertex가 DrawIndexed에서 더해지므로,
	// 각 배치의 index는 0부터 시작해도 됨
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

    // 버퍼 크기 초과 시 재할당
    if (Vertices.size() > MaxVertexCount || Indices.size() > MaxIndexCount)
    {
        MaxVertexCount = static_cast<uint32>(Vertices.size()) * 2;
        MaxIndexCount  = static_cast<uint32>(Indices.size())  * 2;
        CreateBuffers();
    }

    // VB 업로드
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, Vertices.data(), sizeof(FTextureVertex) * Vertices.size());
    Context->Unmap(VertexBuffer, 0);

    // IB 업로드
    if (FAILED(Context->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, Indices.data(), sizeof(uint32) * Indices.size());
    Context->Unmap(IndexBuffer, 0);

    // 셰이더 바인딩
    SubUVShader.Bind(Context);

    uint32 stride = sizeof(FTextureVertex), offset = 0;
    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
    Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->PSSetSamplers(0, 1, &SamplerState);

    // ResourceManager 소유 SRV 바인딩
    // Context->PSSetShaderResources(0, 1, &SRV);
	for (const FSRVBatch& Batch : Batches)
	{
		if (!Batch.SRV || Batch.IndexCount == 0) continue;

		// SRV만 교체 (나머지 상태는 유지)
		Context->PSSetShaderResources(0, 1, &Batch.SRV);

		// DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation)
		//   - StartIndexLocation: IB 내 이 배치의 index 시작 오프셋
		//   - BaseVertexLocation: VB 내 이 배치의 vertex 시작 오프셋
		//     → index 값에 이 값이 더해져서 실제 vertex를 참조
		Context->DrawIndexed(
			Batch.IndexCount,      // 이 배치의 인덱스 수
			Batch.IndexStart,      // IB 내 시작 위치
			Batch.BaseVertex       // VB 내 시작 위치 (index에 더해짐)
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
