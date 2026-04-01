#include "SubUVBatcher.h"

#include "Core/CoreTypes.h"
#include "Render/Resource/ShaderManager.h"

void FSubUVBatcher::Create(ID3D11Device* InDevice)
{
    CreateBuffers(InDevice, 256, sizeof(FTextureVertex), 384);
    if (!Device) return;

    // Sampler — Linear 필터 (스프라이트는 부드럽게)
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    Device->CreateSamplerState(&sampDesc, &SamplerState);

}

void FSubUVBatcher::Release()
{
    Clear();

    if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }

    ReleaseBuffers();
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

void FSubUVBatcher::DrawBatch(ID3D11DeviceContext* Context)
{
    if (Vertices.empty()) return;

    const uint32 VertexCount = static_cast<uint32>(Vertices.size());
    const uint32 IndexCount = static_cast<uint32>(Indices.size());

    VB.EnsureCapacity(Device, VertexCount);
    IB.EnsureCapacity(Device, IndexCount);
    if (!VB.Update(Context, Vertices.data(), VertexCount)) return;
    if (!IB.Update(Context, Indices.data(), IndexCount)) return;

    FShaderManager::Get().GetShader(EShaderType::SubUV)->Bind(Context);
    VB.Bind(Context);
    IB.Bind(Context);
    Context->PSSetSamplers(0, 1, &SamplerState);

    for (const FSRVBatch& Batch : Batches)
    {
        if (!Batch.SRV || Batch.IndexCount == 0) continue;
        Context->PSSetShaderResources(0, 1, &Batch.SRV);
        Context->DrawIndexed(Batch.IndexCount, Batch.IndexStart, Batch.BaseVertex);
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
