#include "Render/Proxy/DecalSceneProxy.h"
#include "Component/DecalComponent.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Runtime/Engine.h"

namespace
{
	struct FDecalConstants
	{
		FVector4 Color;
	};
}

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ExtraCB.Buffer = new FConstantBuffer();
	ExtraCB.Slot = 5;	// 5번 상수버퍼 슬롯 할당
	ExtraCB.Size = sizeof(FVector4);	// Color 1개만 사용
	// 최초 1회 초기화
	UpdateMesh();
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::UpdateMaterial()
{
	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp) return;
	
	DecalTexture = DecalComp->GetTexture();
	if (!SectionDraws.empty())
	{
		SectionDraws[0].DiffuseSRV = DecalTexture ? DecalTexture->SRV : nullptr;
	}
	
	auto& CB = ExtraCB.Bind<FDecalConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Decal, sizeof(FDecalConstants)),
		ECBSlot::Decal);
	CB.Color = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp) return;

	// 1. 컴포넌트가 계산해둔 합쳐진 메쉬 데이터(CombinedDecalMeshData)를 가져옵니다.
	const TMeshData<FVertexPNCT>* NewMeshData = DecalComp->GetDecalMeshData();

	// 2. 데이터가 비어있으면 렌더링하지 않도록 처리
	if (NewMeshData == nullptr || NewMeshData->Vertices.empty())
	{
		this->MeshBuffer = nullptr;
		this->SectionDraws.clear();
		return;
	}

	// 3. FMeshBuffer를 D3D11Device를 통해 생성 및 업데이트 (GPU 버퍼 할당)
	// ID3D11Device는 GEngine이나 FEngineContext 등을 통해 가져올 수 있습니다.
	DecalDynamicMeshBuffer.Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), *NewMeshData);

	// 4. 기반 클래스(FPrimitiveSceneProxy)의 포인터가 내 동적 버퍼를 가리키게 합니다.
	this->MeshBuffer = &DecalDynamicMeshBuffer;

	// 5. SectionDraws 세팅 (렌더러가 이 정보를 보고 DrawIndexed를 호출함)
	this->SectionDraws.clear();
	FMeshSectionDraw DrawCmd;
	DrawCmd.FirstIndex = 0;
	DrawCmd.IndexCount = NewMeshData->Indices.size();
	DrawCmd.DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	if (DecalTexture)
	{
		DrawCmd.DiffuseSRV = DecalTexture->SRV;
	}

	this->SectionDraws.push_back(DrawCmd);

	this->Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	this->Pass = ERenderPass::Decal;
	UpdateSortKey();
}
