#include "DecalComponent.h"

#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/DebugDraw/DrawDebugHelpers.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "UI/EditorConsoleWidget.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

namespace
{
	struct FClipPlane
	{
		FVector Normal;
		float Distance;

		bool IsInside(const FVector& Point) const
		{
			return Point.Dot(Normal) >= Distance;
		}

		float GetT(const FVector& A, const FVector& B) const	
		{
			float Da = A.Dot(Normal) - Distance;
			float Db = B.Dot(Normal) - Distance;
			return Da / (Da - Db);
		}
	};

	// Sutherland-Hodgman clipping for a single plane
	void ClipAgainstPlane(const TArray<FVertexPNCT>& InVertices, const FClipPlane& Plane, TArray<FVertexPNCT>& OutVertices)
	{
		OutVertices.clear();
		if (InVertices.empty()) return;

		for (size_t i = 0; i < InVertices.size(); ++i)
		{
			const FVertexPNCT& V1 = InVertices[i];
			const FVertexPNCT& V2 = InVertices[(i + 1) % InVertices.size()];

			bool Inside1 = Plane.IsInside(V1.Position);
			bool Inside2 = Plane.IsInside(V2.Position);

			if (Inside1 && Inside2)
			{
				OutVertices.push_back(V2);
			}
			else if (Inside1 && !Inside2)
			{
				float T = Plane.GetT(V1.Position, V2.Position);
				FVertexPNCT IntersectV;
				IntersectV.Position = V1.Position + (V2.Position - V1.Position) * T;
				IntersectV.Normal = (V1.Normal + (V2.Normal - V1.Normal) * T).Normalized();
				IntersectV.Color = V1.Color + (V2.Color - V1.Color) * T;
				OutVertices.push_back(IntersectV);
			}
			else if (!Inside1 && Inside2)
			{
				float T = Plane.GetT(V1.Position, V2.Position);
				FVertexPNCT IntersectV;
				IntersectV.Position = V1.Position + (V2.Position - V1.Position) * T;
				IntersectV.Normal = (V1.Normal + (V2.Normal - V1.Normal) * T).Normalized();
				IntersectV.Color = V1.Color + (V2.Color - V1.Color) * T;
				OutVertices.push_back(IntersectV);
				OutVertices.push_back(V2);
			}
		}
	}
}

// 메시를 Convex (=OBB of DecalComponent)로 자르기
TMeshData<FVertexPNCT> SutherlandHodgman(const FStaticMesh* InMesh, const FMatrix& LocalToDecalMatrix)
{
	TMeshData<FVertexPNCT> Result;
	if (!InMesh) return Result;

	// Decal 공간(Unit Cube -0.5 ~ 0.5)에서의 클리핑 평면 정의
	static const FClipPlane Planes[] = {
		{ FVector( 1,  0,  0), -0.5f }, { FVector(-1,  0,  0), -0.5f },
		{ FVector( 0,  1,  0), -0.5f }, { FVector( 0, -1,  0), -0.5f },
		{ FVector( 0,  0,  1), -0.5f }, { FVector( 0,  0, -1), -0.5f }
	};

	FMatrix NormalMat = LocalToDecalMatrix;
	NormalMat.M[3][0] = 0.0f; NormalMat.M[3][1] = 0.0f; NormalMat.M[3][2] = 0.0f;

	// 삼각형 단위로 클리핑 수행
	for (size_t i = 0; i < InMesh->Indices.size(); i += 3)
	{
		TArray<FVertexPNCT> Polygon;
		for (int j = 0; j < 3; ++j)
		{
			const FNormalVertex& NV = InMesh->Vertices[InMesh->Indices[i + j]];
			FVertexPNCT Vert;

			// 타겟 로컬 -> 타겟 월드 -> 데칼 로컬 공간으로 변환
			Vert.Position = NV.pos * LocalToDecalMatrix;
			Vert.Normal = (NV.normal * NormalMat).Normalized();
			Vert.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
			Vert.UV = FVector2(0, 0); // 계산 후 적용

			Polygon.push_back(Vert);
		}

		TArray<FVertexPNCT> ClippedPolygon;
		TArray<FVertexPNCT> TempPolygon;

		ClippedPolygon = Polygon;
		for (const auto& Plane : Planes)
		{
			ClipAgainstPlane(ClippedPolygon, Plane, TempPolygon);
			ClippedPolygon = TempPolygon;
			if (ClippedPolygon.empty()) break;
		}

		if (ClippedPolygon.size() >= 3)
		{
			uint32 BaseIndex = static_cast<uint32>(Result.Vertices.size());
			for (auto& Vert : ClippedPolygon)
			{
				// Decal space is -0.5 to 0.5. U and V map to Y and Z.
				Vert.UV.X = Vert.Position.Y + 0.5f;
				Vert.UV.Y = 0.5f - Vert.Position.Z; 

				// 타겟 메쉬와 완전히 겹쳐서 발생하는 Z-Fighting을 막기 위해 로컬 법선 방향으로 아주 미세하게 띄워줌
				Vert.Position += Vert.Normal * 0.0005f;

				Result.Vertices.push_back(Vert);
			}
			for (size_t j = 1; j < ClippedPolygon.size() - 1; ++j)
			{
				Result.Indices.push_back(BaseIndex);
				Result.Indices.push_back(BaseIndex + (uint32)j);
				Result.Indices.push_back(BaseIndex + (uint32)j + 1);
			}
		}
	}

	return Result;
}

void UDecalComponent::UpdateDecalMesh()
{
	UpdateOBBFromTransform();

	TArray<UPrimitiveComponent*> OverlappingPrimitives;
	GetOwner()->GetWorld()->GetPartition().QueryFrustumAllPrimitive(OBB, OverlappingPrimitives);

	// ===== Calculate Mesh =====
	DecalMeshData.Vertices.clear();
	DecalMeshData.Indices.clear();

	FMatrix InvWorld = GetWorldMatrix().GetInverse();

	for (auto PrimitiveComp : OverlappingPrimitives)
	{
		if (PrimitiveComp->GetOwner() == this->GetOwner()) continue;

		// 오로지 StaticMeshComponent만 데칼의 대상으로 허용
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(PrimitiveComp);
		if (!SMC) continue;

		// UStaticMeshAsset으로부터 직접 Raw Data 추출
		UStaticMesh* StaticMeshAsset = SMC->GetStaticMesh();
		if (!StaticMeshAsset) continue;

		FStaticMesh* RawMesh = StaticMeshAsset->GetStaticMeshAsset();
		if (!RawMesh) continue;

		FMatrix LocalToDecalMatrix = SMC->GetWorldMatrix() * InvWorld;
		TMeshData<FVertexPNCT> Sliced = SutherlandHodgman(RawMesh, LocalToDecalMatrix);

		uint32 IndexOffset = static_cast<uint32>(DecalMeshData.Vertices.size());
		for (const auto& Vertex : Sliced.Vertices)
			DecalMeshData.Vertices.push_back(Vertex);
		for (auto Index : Sliced.Indices)
			DecalMeshData.Indices.push_back(Index + IndexOffset);
	}

	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UpdateDecalMesh();

	// ===== Debug line draw ===== 
	DrawDebugBox();
}FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
	return new FDecalSceneProxy(this);
}

void UDecalComponent::DrawDebugBox()
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	FVector P[8] = {
		FVector(-0.5f, -0.5f, -0.5f) * WorldMatrix,
		FVector( 0.5f, -0.5f, -0.5f) * WorldMatrix,
		FVector( 0.5f,  0.5f, -0.5f) * WorldMatrix,
		FVector(-0.5f,  0.5f, -0.5f) * WorldMatrix,
		FVector(-0.5f, -0.5f,  0.5f) * WorldMatrix,
		FVector( 0.5f, -0.5f,  0.5f) * WorldMatrix,
		FVector( 0.5f,  0.5f,  0.5f) * WorldMatrix,
		FVector(-0.5f,  0.5f,  0.5f) * WorldMatrix
	};

	UWorld* World = GetOwner()->GetWorld();
	
	// 하단면
	DrawDebugLine(World, P[0], P[1], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[1], P[2], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[2], P[3], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[3], P[0], FColor::Green(), 0.0f);
	
	// 상단면
	DrawDebugLine(World, P[4], P[5], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[5], P[6], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[6], P[7], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[7], P[4], FColor::Green(), 0.0f);
	
	// 수직선
	DrawDebugLine(World, P[0], P[4], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[1], P[5], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[2], P[6], FColor::Green(), 0.0f);
	DrawDebugLine(World, P[3], P[7], FColor::Green(), 0.0f);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({"Color", EPropertyType::Vec4, &Color });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		SetTexture(TextureName);
		MarkRenderStateDirty();
	}
	if (strcmp(PropertyName, "Color") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::UpdateOBBFromTransform()
{
	OBB.UpdateAsOBB(GetWorldMatrix());
}

void UDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	UpdateDecalMesh();
}
