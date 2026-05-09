#include "FbxImporter.h"
#include "Asset/StaticMeshTypes.h"
#include "Core/Logging/Log.h"
#include "Core/PlatformTime.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cfloat>

using namespace fbxsdk;

namespace
{
	static FVector ToFVector(const FbxVector4& V)
	{
		return FVector(static_cast<float>(V[0]), static_cast<float>(V[1]), static_cast<float>(V[2]));
	}

	static FVector2 ToFVector2(const FbxVector2& V)
	{
		// OBJ 로더와 동일하게 V 좌표 뒤집기
		return FVector2(static_cast<float>(V[0]), 1.0f - static_cast<float>(V[1]));
	}

	static void GetTangentBitangent(FVector& OutT, FVector& OutB,
		const FVector& P0, const FVector& P1, const FVector& P2,
		const FVector2& UV0, const FVector2& UV1, const FVector2& UV2)
	{
		FVector E1 = P1 - P0;
		FVector E2 = P2 - P0;
		FVector2 dUV1 = UV1 - UV0;
		FVector2 dUV2 = UV2 - UV0;
		float Det = dUV1.X * dUV2.Y - dUV1.Y * dUV2.X;
		float r = (fabs(Det) > 1e-8f) ? (1.0f / Det) : 0.0f;

		OutT = (E1 * dUV2.Y - E2 * dUV1.Y) * r;
		OutB = (E2 * dUV1.X - E1 * dUV2.X) * r;
	}
}

FStaticMesh* FFbxImporter::Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
	const double StartTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Start loading FBX: %s", Path.c_str());

	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
		return nullptr;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
	if (!Scene)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene");
		Manager->Destroy();
		return nullptr;
	}

	if (!ImportScene(Path, Manager, Scene))
	{
		Manager->Destroy();
		return nullptr;
	}

	// Triangulate 후 메시 처리
	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, /*pReplace=*/true);

	FStaticMesh* StaticMesh = new FStaticMesh();
	StaticMesh->PathFileName = Path;

	if (FbxNode* RootNode = Scene->GetRootNode())
	{
		for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
		{
			CollectMeshes(RootNode->GetChild(i), StaticMesh);
		}
	}

	Manager->Destroy();

	if (StaticMesh->Vertices.empty() || StaticMesh->Indices.empty())
	{
		UE_LOG_ERROR("[FbxImporter] No geometry found in FBX: %s", Path.c_str());
		delete StaticMesh;
		return nullptr;
	}

	if (LoadOptions.bNormalizeToUnitCube)
	{
		UE_LOG("[FbxImporter] NormalizeToUnitCube enabled: %s", Path.c_str());
		NormalizePositionsToUnitCube(StaticMesh);
	}

	StaticMesh->LocalBounds = BuildLocalBounds(StaticMesh);

	ComputeTangents(StaticMesh);

	UE_LOG("[FbxImporter] FBX Loaded: %s (Vertices: %zu, Indices: %zu, Sections: %zu, Slots: %zu)",
		Path.c_str(),
		StaticMesh->Vertices.size(),
		StaticMesh->Indices.size(),
		StaticMesh->Sections.size(),
		StaticMesh->Slots.size());

	const double EndTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Loaded %s in %.3f sec", Path.c_str(), EndTime - StartTime);

	return StaticMesh;
}

bool FFbxImporter::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("fbx") || Extension == FString(".fbx") ||
		   Extension == FString("FBX") || Extension == FString(".FBX");
}

FString FFbxImporter::GetLoaderName() const
{
	return FString{ "FFbxImporter" };
}

bool FFbxImporter::ImportScene(const FString& Path, FbxManager* Manager, FbxScene* Scene)
{
	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(Path.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_ERROR("[FbxImporter] Initialize failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
		Importer->Destroy();
		return false;
	}

	const bool bResult = Importer->Import(Scene);
	if (!bResult)
	{
		UE_LOG_ERROR("[FbxImporter] Import failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
	}

	Importer->Destroy();

	if (bResult)
	{
		// Engine import policy: left-handed, Z-up, X-forward, meter.
		// FBX SDK가 mesh/transform/anim까지 일관되게 변환해주므로 정점 단계에서 축 swap 금지.
		const FbxAxisSystem TargetAxis(
			FbxAxisSystem::eZAxis,
			FbxAxisSystem::eParityOdd,
			FbxAxisSystem::eLeftHanded);
		TargetAxis.DeepConvertScene(Scene);

		FbxSystemUnit::m.ConvertScene(Scene);
	}

	return bResult;
}

void FFbxImporter::CollectMeshes(FbxNode* Node, FStaticMesh* InStaticMesh)
{
	if (!Node) return;

	if (FbxNodeAttribute* Attr = Node->GetNodeAttribute())
	{
		if (Attr->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			ProcessMesh(static_cast<FbxMesh*>(Attr), InStaticMesh);
		}
	}

	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		CollectMeshes(Node->GetChild(i), InStaticMesh);
	}
}

void FFbxImporter::ProcessMesh(FbxMesh* Mesh, FStaticMesh* InStaticMesh)
{
	if (!Mesh || Mesh->GetPolygonCount() <= 0)
	{
		return;
	}

	FbxNode* OwnerNode = Mesh->GetNode();

	// FbxAxisSystem/FbxSystemUnit::ConvertScene은 노드 transform에 변환을 baked함.
	// → control point에 GlobalTransform * GeometricTransform을 적용해야 단위/축이 반영됨.
	FbxAMatrix VertexTransform;
	FbxAMatrix NormalTransform;
	if (OwnerNode)
	{
		const FbxVector4 T = OwnerNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		const FbxVector4 R = OwnerNode->GetGeometricRotation(FbxNode::eSourcePivot);
		const FbxVector4 S = OwnerNode->GetGeometricScaling(FbxNode::eSourcePivot);
		FbxAMatrix GeomTransform;
		GeomTransform.SetTRS(T, R, S);

		const FbxAMatrix GlobalTransform = OwnerNode->EvaluateGlobalTransform();
		VertexTransform = GlobalTransform * GeomTransform;

		// Normal은 회전·스케일만 — translation 제거
		NormalTransform = VertexTransform;
		NormalTransform.SetT(FbxVector4(0, 0, 0, 0));
	}

	const FbxVector4* ControlPoints = Mesh->GetControlPoints();
	if (!ControlPoints) return;

	// 머티리얼 매핑 모드 확인 (per-polygon으로 가정, 그 외엔 단일 슬롯으로 처리)
	FbxLayerElementArrayTemplate<int32>* MaterialIndices = nullptr;
	FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eByPolygon;
	if (Mesh->GetElementMaterial())
	{
		MaterialIndices = &Mesh->GetElementMaterial()->GetIndexArray();
		MaterialMappingMode = Mesh->GetElementMaterial()->GetMappingMode();
	}

	// 슬롯별 인덱스 임시 저장 (OBJ 로더와 동일한 패턴)
	TArray<TArray<uint32>> SlotIndices;

	const int32 PolygonCount = Mesh->GetPolygonCount();
	for (int32 PolyIdx = 0; PolyIdx < PolygonCount; ++PolyIdx)
	{
		// Triangulate 이후이므로 PolygonSize == 3 가정
		const int32 PolygonSize = Mesh->GetPolygonSize(PolyIdx);
		if (PolygonSize != 3) continue;

		// 머티리얼 슬롯 결정
		FString MaterialName = "DefaultWhite";
		if (MaterialIndices && OwnerNode)
		{
			int32 MatIdx = 0;
			if (MaterialMappingMode == FbxGeometryElement::eByPolygon &&
				PolyIdx < MaterialIndices->GetCount())
			{
				MatIdx = MaterialIndices->GetAt(PolyIdx);
			}
			else if (MaterialMappingMode == FbxGeometryElement::eAllSame &&
				MaterialIndices->GetCount() > 0)
			{
				MatIdx = MaterialIndices->GetAt(0);
			}

			if (MatIdx >= 0 && MatIdx < OwnerNode->GetMaterialCount())
			{
				if (FbxSurfaceMaterial* SurfMat = OwnerNode->GetMaterial(MatIdx))
				{
					MaterialName = FString(SurfMat->GetName());
				}
			}
		}

		const int32 SlotIdx = GetOrAddMaterialSlot(InStaticMesh, MaterialName);
		if (SlotIdx >= static_cast<int32>(SlotIndices.size()))
		{
			SlotIndices.resize(SlotIdx + 1);
		}

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);

			FNormalVertex Vertex = {};

			// Position
			FbxVector4 Pos = ControlPoints[CtrlPointIdx];
			Pos = VertexTransform.MultT(Pos);
			Vertex.Position = ToFVector(Pos);

			// Normal
			FbxVector4 Normal(0, 0, 1, 0);
			if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
			{
				Normal[3] = 0.0;  // direction vector — translation은 무시
				Normal = NormalTransform.MultT(Normal);
				Vertex.Normal = ToFVector(Normal);
				const float Len = Vertex.Normal.Size();
				if (Len > 1e-6f) Vertex.Normal = Vertex.Normal / Len;
			}
			else
			{
				Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
			}

			// UV (첫 번째 채널만 사용)
			Vertex.UVs = FVector2(0.0f, 0.0f);
			if (Mesh->GetElementUVCount() > 0)
			{
				FbxStringList UVNames;
				Mesh->GetUVSetNames(UVNames);
				if (const char* UVName = UVNames.GetStringAt(0))
				{
					FbxVector2 UV;
					bool bUnmapped = false;
					if (Mesh->GetPolygonVertexUV(PolyIdx, Corner, UVName, UV, bUnmapped))
					{
						Vertex.UVs = ToFVector2(UV);
					}
				}
			}

			Vertex.Color = FColor{ 1.0f, 1.0f, 1.0f, 1.0f };

			const uint32 NewIndex = static_cast<uint32>(InStaticMesh->Vertices.size());
			InStaticMesh->Vertices.push_back(Vertex);
			SlotIndices[SlotIdx].push_back(NewIndex);
		}
	}

	// 슬롯별 인덱스를 Mesh.Indices에 합치고 Section 생성
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(SlotIndices.size()); ++SlotIdx)
	{
		TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
		if (IndicesPerSlot.empty()) continue;

		FStaticMeshSection NewSection;
		NewSection.StartIndex = static_cast<uint32>(InStaticMesh->Indices.size());
		NewSection.IndexCount = static_cast<uint32>(IndicesPerSlot.size());
		NewSection.MaterialSlotIndex = SlotIdx;

		InStaticMesh->Indices.insert(
			InStaticMesh->Indices.end(),
			IndicesPerSlot.begin(),
			IndicesPerSlot.end());

		InStaticMesh->Sections.push_back(NewSection);
	}
}

int32 FFbxImporter::GetOrAddMaterialSlot(FStaticMesh* InStaticMesh, const FString& MaterialName)
{
	const FString SlotName = MaterialName.empty() ? FString("DefaultWhite") : MaterialName;

	for (int32 i = 0; i < static_cast<int32>(InStaticMesh->Slots.size()); ++i)
	{
		if (InStaticMesh->Slots[i].SlotName == SlotName)
		{
			return i;
		}
	}

	FStaticMeshMaterialSlot NewSlot;
	NewSlot.SlotName = SlotName;
	NewSlot.Material = nullptr;
	InStaticMesh->Slots.push_back(NewSlot);
	return static_cast<int32>(InStaticMesh->Slots.size() - 1);
}

FAABB FFbxImporter::BuildLocalBounds(FStaticMesh* InStaticMesh) const
{
	FAABB Bounds;
	Bounds.Reset();

	for (const FNormalVertex& Vertex : InStaticMesh->Vertices)
	{
		Bounds.Expand(Vertex.Position);
	}

	return Bounds;
}

void FFbxImporter::NormalizePositionsToUnitCube(FStaticMesh* InStaticMesh)
{
	if (InStaticMesh->Vertices.empty()) return;

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FNormalVertex& V : InStaticMesh->Vertices)
	{
		Min.X = std::min(Min.X, V.Position.X);
		Min.Y = std::min(Min.Y, V.Position.Y);
		Min.Z = std::min(Min.Z, V.Position.Z);
		Max.X = std::max(Max.X, V.Position.X);
		Max.Y = std::max(Max.Y, V.Position.Y);
		Max.Z = std::max(Max.Z, V.Position.Z);
	}

	const FVector Center = (Min + Max) * 0.5f;
	const FVector Size = Max - Min;
	const float MaxDim = std::max(Size.X, std::max(Size.Y, Size.Z));
	if (MaxDim <= 1e-6f) return;

	const float Scale = 1.0f / MaxDim;
	for (FNormalVertex& V : InStaticMesh->Vertices)
	{
		V.Position = (V.Position - Center) * Scale;
	}
}

void FFbxImporter::ComputeTangents(FStaticMesh* InStaticMesh)
{
	const uint64 VertexCount = InStaticMesh->Vertices.size();
	TArray<FVector> TangentAcc(VertexCount, FVector(0, 0, 0));
	TArray<FVector> BitangentAcc(VertexCount, FVector(0, 0, 0));

	const TArray<uint32>& Idx = InStaticMesh->Indices;
	for (uint64 i = 0; i + 2 < Idx.size(); i += 3)
	{
		const uint32 I0 = Idx[i], I1 = Idx[i + 1], I2 = Idx[i + 2];
		const FNormalVertex& V0 = InStaticMesh->Vertices[I0];
		const FNormalVertex& V1 = InStaticMesh->Vertices[I1];
		const FNormalVertex& V2 = InStaticMesh->Vertices[I2];

		FVector T, B;
		GetTangentBitangent(T, B, V0.Position, V1.Position, V2.Position,
			V0.UVs, V1.UVs, V2.UVs);
		TangentAcc[I0] += T; TangentAcc[I1] += T; TangentAcc[I2] += T;
		BitangentAcc[I0] += B; BitangentAcc[I1] += B; BitangentAcc[I2] += B;
	}

	for (uint64 i = 0; i < VertexCount; ++i)
	{
		const FVector& N = InStaticMesh->Vertices[i].Normal;
		FVector T = TangentAcc[i];

		// Gram-Schmidt
		T = (T - N * FVector::DotProduct(N, T));
		const float Len = T.Size();
		T = (Len > 1e-6f) ? T / Len : FVector(1, 0, 0);

		const FVector ExpectedB = FVector::CrossProduct(N, T);
		const float Sign = (FVector::DotProduct(ExpectedB, BitangentAcc[i]) < 0.0f) ? -1.0f : 1.0f;

		InStaticMesh->Vertices[i].Tangent = FVector4(T.X, T.Y, T.Z, Sign);
	}
}
