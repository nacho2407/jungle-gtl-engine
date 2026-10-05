#include "MeshBufferManager.h"

#include "Asset/SkeletalMesh.h"
#include "Asset/StaticMesh.h"
#include "Component/ProceduralMeshComponent.h"
#include "Core/Logging/SkinningStats.h"
#include "Render/Scene/RenderCommand.h"

#include <chrono>

namespace
{
	FMeshData CreateBillboardQuadMeshData()
	{
		FMeshData QuadMeshData;
		FColor DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f, -0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), DefaultColor, 0 });

		QuadMeshData.Indices = { 0, 1, 2, 0, 2, 3 };
		return QuadMeshData;
	}

	bool UpdateSkeletalMeshBufferVertices(ID3D11Device* Device, FMeshBuffer& Buffer, const TArray<FSkeletalMeshVertex>& Vertices)
	{
		if (!Device)
		{
			return false;
		}

		ID3D11DeviceContext* DeviceContext = nullptr;
		Device->GetImmediateContext(&DeviceContext);
		if (!DeviceContext)
		{
			return false;
		}

		const auto UploadStart = std::chrono::steady_clock::now();
		Buffer.UpdateDynamicVertices(DeviceContext, Vertices);
		const auto UploadEnd = std::chrono::steady_clock::now();
		FSkinningStats::Get().AddCPUSkinnedVertexBufferUpload(
			std::chrono::duration<double, std::milli>(UploadEnd - UploadStart).count(),
			static_cast<uint64>(Vertices.size()) * sizeof(FSkeletalMeshVertex));
		DeviceContext->Release();

		return true;
	}
}

void FMeshBufferManager::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
	const FMeshData QuadMeshData = CreateBillboardQuadMeshData();

	MeshBufferMap[EPrimitiveType::EPT_TransGizmo].Create(InDevice, FEditorMeshLibrary::GetTranslationGizmo());
	MeshBufferMap[EPrimitiveType::EPT_RotGizmo].Create(InDevice, FEditorMeshLibrary::GetRotationGizmo()); 
	MeshBufferMap[EPrimitiveType::EPT_ScaleGizmo].Create(InDevice, FEditorMeshLibrary::GetScaleGizmo());
	MeshBufferMap[EPrimitiveType::EPT_Billboard].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_SubUV].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_Text].Create(InDevice, QuadMeshData);
    MeshBufferMap[EPrimitiveType::EPT_ProceduralMesh].Create(InDevice, QuadMeshData);
}


void FMeshBufferManager::Release()
{
	for (auto& pair : MeshBufferMap)
    {
        pair.second.Release();
    }
    MeshBufferMap.clear();

    for (int32 i = 0; i < MAX_LOD; ++i) 
    {
        for (auto& pair : StaticMeshBufferMap[i])
        {
            pair.second.Release();
        }
        StaticMeshBufferMap[i].clear();
    }

	for (auto& pair : ProcMeshBufferMap)
	{
        pair.second.Release();
	}

	ProcMeshBufferMap.clear();

	for (auto& pair : CPUSkeletalMeshBufferMap)
	{
		pair.second.Release();
	}

	CPUSkeletalMeshBufferMap.clear();
	CPUSkeletalMeshSourceMap.clear();

	for (auto& pair : GPUSkeletalMeshBufferMap)
	{
		pair.second.Release();
	}

	GPUSkeletalMeshBufferMap.clear();

	for (auto& pair : GPUSkeletalBoneMatrixBufferMap)
	{
		pair.second.Release();
	}

	GPUSkeletalBoneMatrixBufferMap.clear();
    
    Device = nullptr;
}

//	MeshBuffer는 VB, IB를 모두 포함하고 있습니다.
FMeshBuffer& FMeshBufferManager::GetMeshBuffer(EPrimitiveType InPrimitiveType)
{
	auto it = MeshBufferMap.find(InPrimitiveType);
	if (it != MeshBufferMap.end())
	{
		return it->second;
	}
	
	//	존재하지 않는 PrimitiveType이 요청된 경우, Billboard Quad를 기본 반환합니다.
	return MeshBufferMap.at(EPrimitiveType::EPT_Billboard);
}

FMeshBuffer* FMeshBufferManager::GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel)
{
	if (!Device || !StaticMeshAsset || !StaticMeshAsset->HasValidMeshData())
    {
        return nullptr;
    }

    // 1. LOD 레벨 안전장치 (Crash 방지)
    // 요청한 LOD가 실제 가진 것보다 크면, 가장 최하위(마지막) LOD로 강제 조정합니다.
    int32 ValidLODCount = StaticMeshAsset->GetValidLODCount();
    if (LODLevel >= ValidLODCount)
    {
        LODLevel = ValidLODCount - 1;
    }
    if (LODLevel < 0) LODLevel = 0;

    auto& TargetMap = StaticMeshBufferMap[LODLevel];

    auto It = TargetMap.find(StaticMeshAsset);
    if (It != TargetMap.end())
    {
        return &It->second;
    }

    const FStaticMesh* LODData = StaticMeshAsset->GetMeshData(LODLevel);
    if (!LODData)
    {
        return nullptr;
    }

    const TArray<FNormalVertex>& Vertices = LODData->Vertices;
    const TArray<uint32>&        Indices  = LODData->Indices;

    if (Vertices.empty() || Indices.empty())
    {
        return nullptr;
    }

    FMeshBuffer& NewBuffer = TargetMap[StaticMeshAsset];
    NewBuffer.CreateImmutableVertices(Device, Vertices, Indices);
    

    return &NewBuffer;
}

FMeshBuffer* FMeshBufferManager::GetProcMeshBuffer(uint32 ProcMeshCompUUID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
    if (Vertices.empty() || Indices.empty())
    {
        return nullptr;
    }

    auto It = ProcMeshBufferMap.find(ProcMeshCompUUID);
    if (It != ProcMeshBufferMap.end())
    {
        FMeshBuffer& Existing = It->second;
        if (Existing.IsValid())
        {
            const uint32 ExistingVertexCount = Existing.GetVertexBuffer().GetVertexCount();
            const uint32 ExistingIndexCount = Existing.GetIndexBuffer().GetIndexCount();
			// Size 달라짐을 통해 유효 여부 확인
            if (ExistingVertexCount == static_cast<uint32>(Vertices.size()) && ExistingIndexCount == static_cast<uint32>(Indices.size()))
            {
                return &Existing;
            }
            // recreate if size differs
            Existing.Release();
        }
    }

    FMeshBuffer& NewBuffer = ProcMeshBufferMap[ProcMeshCompUUID];
    NewBuffer.CreateImmutableVertices(Device, Vertices, Indices);

    return &NewBuffer;
}

FMeshBuffer* FMeshBufferManager::GetCPUSkeletalMeshBuffer(uint32 SkeletalMeshCompUUID, const USkeletalMesh* SkeletalMeshAsset, const TArray<FSkeletalMeshVertex>& Vertices, const TArray<uint32>& Indices, bool bNeedsUpload)
{
	if (!Device || !SkeletalMeshAsset || Vertices.empty() || Indices.empty())
	{
		return nullptr;
	}

	auto It = CPUSkeletalMeshBufferMap.find(SkeletalMeshCompUUID);
	if (It != CPUSkeletalMeshBufferMap.end())
	{
		FMeshBuffer& Existing = It->second;
		if (Existing.IsValid())
		{
			auto SourceIt = CPUSkeletalMeshSourceMap.find(SkeletalMeshCompUUID);
			const bool bSameSourceMesh = SourceIt != CPUSkeletalMeshSourceMap.end() && SourceIt->second == SkeletalMeshAsset;
			const uint32 ExistingVertexCount = Existing.GetVertexBuffer().GetVertexCount();
			const uint32 ExistingIndexCount = Existing.GetIndexBuffer().GetIndexCount();
			const bool bSameSize =
				ExistingVertexCount == static_cast<uint32>(Vertices.size()) &&
				ExistingIndexCount == static_cast<uint32>(Indices.size());

			// 개수만 확인하면 mesh 교체 시 vertex / index 개수가 우연히 같은 경우에 index buffer를 잘못 재사용하는 경우가 생길 수 있음~
			if (bSameSourceMesh && bSameSize)
			{
				if (bNeedsUpload)
				{
					UpdateSkeletalMeshBufferVertices(Device, Existing, Vertices);
				}

				return &Existing;
			}

			Existing.Release();
		}
	}

	FMeshBuffer& NewBuffer = CPUSkeletalMeshBufferMap[SkeletalMeshCompUUID];
	NewBuffer.CreateDynamicVertices<FSkeletalMeshVertex>(Device, static_cast<uint32>(Vertices.size()), Indices);
	UpdateSkeletalMeshBufferVertices(Device, NewBuffer, Vertices);
	CPUSkeletalMeshSourceMap[SkeletalMeshCompUUID] = SkeletalMeshAsset;

	return NewBuffer.IsValid() ? &NewBuffer : nullptr;
}

FMeshBuffer* FMeshBufferManager::GetGPUSkeletalMeshBuffer(const USkeletalMesh* SkeletalMeshAsset)
{
	if (!Device || !SkeletalMeshAsset || !SkeletalMeshAsset->HasValidMeshData())
	{
		return nullptr;
	}

	auto It = GPUSkeletalMeshBufferMap.find(SkeletalMeshAsset);
	if (It != GPUSkeletalMeshBufferMap.end())
	{
		return It->second.IsValid() ? &It->second : nullptr;
	}

	const TArray<FSkeletalMeshVertex>& Vertices = SkeletalMeshAsset->GetVertices();
	const TArray<uint32>& Indices = SkeletalMeshAsset->GetIndices();
	if (Vertices.empty() || Indices.empty())
	{
		return nullptr;
	}

	FMeshBuffer& NewBuffer = GPUSkeletalMeshBufferMap[SkeletalMeshAsset];
	NewBuffer.CreateImmutableVertices(Device, Vertices, Indices);

	return NewBuffer.IsValid() ? &NewBuffer : nullptr;
}

FConstantBuffer* FMeshBufferManager::GetGPUSkeletalBoneMatrixBuffer(
	uint32 SkeletalMeshCompUUID,
	const FBoneMatrixConstants& Constants,
	bool bNeedsUpload)
{
	if (!Device)
	{
		return nullptr;
	}

	FConstantBuffer& BoneMatrixBuffer = GPUSkeletalBoneMatrixBufferMap[SkeletalMeshCompUUID];
	if (!BoneMatrixBuffer.GetBuffer())
	{
		BoneMatrixBuffer.Create(Device, sizeof(FBoneMatrixConstants));
		bNeedsUpload = true;
	}

	if (bNeedsUpload)
	{
		ID3D11DeviceContext* DeviceContext = nullptr;
		Device->GetImmediateContext(&DeviceContext);
		if (DeviceContext)
		{
			constexpr uint64 UploadBytes = sizeof(FBoneMatrixConstants);
			const auto UploadStart = std::chrono::steady_clock::now();
			BoneMatrixBuffer.Update(DeviceContext, &Constants, static_cast<uint32>(UploadBytes));
			const auto UploadEnd = std::chrono::steady_clock::now();
			FSkinningStats::Get().AddGPUBoneMatrixUpload(
				std::chrono::duration<double, std::milli>(UploadEnd - UploadStart).count(),
				Constants.BoneCount,
				UploadBytes);
			DeviceContext->Release();
		}
	}

	return BoneMatrixBuffer.GetBuffer() ? &BoneMatrixBuffer : nullptr;
}
