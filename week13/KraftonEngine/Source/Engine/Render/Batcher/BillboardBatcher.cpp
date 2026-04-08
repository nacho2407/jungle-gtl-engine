#include "BillboardBatcher.h"

#include "Core/CoreTypes.h"
#include "Profiling/Stats.h"
#include "Render/Resource/ShaderManager.h"

void FBillboardBatcher::Create(ID3D11Device* InDevice)
{
	CreateBuffers(InDevice, 256, sizeof(FTextureVertex), 384);
	if (!Device) return;

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Device->CreateSamplerState(&sampDesc, &SamplerState);
}

void FBillboardBatcher::Release()
{
	Clear();
	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }
	ReleaseBuffers();
}

void FBillboardBatcher::AddSprite(ID3D11ShaderResourceView* SRV,
								  const FVector& WorldPos,
								  const FVector& CamRight,
								  const FVector& CamUp,
								  const FVector& WorldScale,
								  float Width,
								  float Height)
{
	if (Batches.empty() || Batches.back().SRV != SRV)
	{
		FSRVBatch batch;
		batch.SRV = SRV;
		batch.IndexStart = static_cast<uint32>(Indices.size());
		batch.IndexCount = 0;
		batch.BaseVertex = static_cast<int32>(Vertices.size());
		Batches.push_back(batch);
	}

	const float HalfW = Width  * WorldScale.Y * 0.25f;
	const float HalfH = Height * WorldScale.Z * 0.25f;

	FVector v0 = WorldPos + CamRight * (-HalfW) + CamUp * ( HalfH);
	FVector v1 = WorldPos + CamRight * ( HalfW) + CamUp * ( HalfH);
	FVector v2 = WorldPos + CamRight * (-HalfW) + CamUp * (-HalfH);
	FVector v3 = WorldPos + CamRight * ( HalfW) + CamUp * (-HalfH);

	uint32 LocalBase = static_cast<uint32>(Vertices.size())
		- static_cast<uint32>(Batches.back().BaseVertex);

	// 풀 쿼드 UV (0~1)
	Vertices.push_back({ v0, { 0.0f, 0.0f } });
	Vertices.push_back({ v1, { 1.0f, 0.0f } });
	Vertices.push_back({ v2, { 0.0f, 1.0f } });
	Vertices.push_back({ v3, { 1.0f, 1.0f } });

	Indices.push_back(LocalBase + 0); Indices.push_back(LocalBase + 1); Indices.push_back(LocalBase + 2);
	Indices.push_back(LocalBase + 1); Indices.push_back(LocalBase + 3); Indices.push_back(LocalBase + 2);

	Batches.back().IndexCount += 6;
}

void FBillboardBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
	Batches.clear();
}

void FBillboardBatcher::DrawBatch(ID3D11DeviceContext* Context)
{
	if (Vertices.empty()) return;

	const uint32 VertexCount = static_cast<uint32>(Vertices.size());
	const uint32 IndexCount  = static_cast<uint32>(Indices.size());

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);
	if (!VB.Update(Context, Vertices.data(), VertexCount)) return;
	if (!IB.Update(Context, Indices.data(), IndexCount)) return;

	FShaderManager::Get().GetShader(EShaderType::Billboard)->Bind(Context);
	VB.Bind(Context);
	IB.Bind(Context);
	Context->PSSetSamplers(0, 1, &SamplerState);

	for (const FSRVBatch& Batch : Batches)
	{
		if (!Batch.SRV || Batch.IndexCount == 0) continue;
		Context->PSSetShaderResources(0, 1, &Batch.SRV);
		Context->DrawIndexed(Batch.IndexCount, Batch.IndexStart, Batch.BaseVertex);
		FDrawCallStats::Increment();
	}
}
