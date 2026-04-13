#include "DecalComponent.h"

#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Profiling/Stats.h"
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
			const FVertexPNCT& CurrentV = InVertices[i];
			const FVertexPNCT& NextV = InVertices[(i + 1) % InVertices.size()];

			bool bCurrentInside = Plane.IsInside(CurrentV.Position);
			bool bNextInside = Plane.IsInside(NextV.Position);

			// 1. 시작점이 내부에 있으면 일단 결과에 포함
			if (bCurrentInside)
			{
				OutVertices.push_back(CurrentV);
			}

			// 2. 시작점과 끝점의 내부/외부 상태가 다르면 교차점을 찾아 추가
			if (bCurrentInside != bNextInside)
			{
				float T = Plane.GetT(CurrentV.Position, NextV.Position);
				FVertexPNCT IntersectV;
				IntersectV.Position = CurrentV.Position + (NextV.Position - CurrentV.Position) * T;
				IntersectV.Normal = (CurrentV.Normal + (NextV.Normal - CurrentV.Normal) * T).Normalized();
				IntersectV.Color = CurrentV.Color + (NextV.Color - CurrentV.Color) * T;
				OutVertices.push_back(IntersectV);
			}
		}
	}

	// 메시를 Convex (=OBB of DecalComponent)로 자르기
	TMeshData<FVertexPNCT> SutherlandHodgman(
		const TArray<FNormalVertex>& Vertices, 
		const TArray<uint32>& AllMeshIndices, 
		const TArray<uint32>& TriangleStartIndices, 
		const FMatrix& LocalToDecalMatrix)
	{
		TMeshData<FVertexPNCT> Result;
		if (Vertices.size() == 0 || TriangleStartIndices.size() == 0) return Result;

		// Decal 공간(Unit Cube -0.5 ~ 0.5)에서의 클리핑 평면 정의
		static const FClipPlane Planes[] = {
			{ FVector( 1,  0,  0), -0.5f }, { FVector(-1,  0,  0), -0.5f },
			{ FVector( 0,  1,  0), -0.5f }, { FVector( 0, -1,  0), -0.5f },
			{ FVector( 0,  0,  1), -0.5f }, { FVector( 0,  0, -1), -0.5f }
		};

		FMatrix NormalMat = LocalToDecalMatrix;
		NormalMat.M[3][0] = 0.0f; NormalMat.M[3][1] = 0.0f; NormalMat.M[3][2] = 0.0f;

		// 삼각형 단위로 클리핑 수행
		for (uint32 TriStartIndex : TriangleStartIndices)
		{
			TArray<FVertexPNCT> Polygon;
			for (int j = 0; j < 3; ++j)
			{
				uint32 VertexIndex = AllMeshIndices[TriStartIndex + j];
				const FNormalVertex& NV = Vertices[VertexIndex];
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
} // namespace

// ==============================================
//				Public Functions 
// ==============================================

void UDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	// 인게임 도중에만 Fade 효과 업데이트
	if (TickType == ELevelTick::LEVELTICK_All)
		HandleFade(DeltaTime);
	UpdateDecalMesh();

	// Debug line draw
	DrawDebugBox();
}

FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
	return new FDecalSceneProxy(this);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({"Color", EPropertyType::Vec4, &Color });
	OutProps.push_back({"FadeInDelay", EPropertyType::Float, &FadeInDelay});
	OutProps.push_back({"FadeInDuration", EPropertyType::Float, &FadeInDuration});
	OutProps.push_back({"FadeOutDelay", EPropertyType::Float, &FadeOutDelay});
	OutProps.push_back({"FadeOutDuration", EPropertyType::Float, &FadeOutDuration});
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

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << TextureName;
	Ar << Color;
	Ar << FadeInDelay;
	Ar << FadeInDuration;
	Ar << FadeOutDelay;
	Ar << FadeOutDuration;
}

void UDecalComponent::PostDuplicate()
{
	// 컴포넌트 복제 후 텍스쳐 재로딩
	if (TextureName.IsValid())
	{
		SetTexture(TextureName);
	}
	MarkProxyDirty(EDirtyFlag::Material);
}

FVector4 UDecalComponent::GetColor() const
{
	FVector4 OutColor = this->Color; 
	OutColor.A = OutColor.A * Clamp(FadeOpacity, 0, 1); 
	return OutColor;
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::UpdateOBBFromTransform()
{
	ConvexVolume.UpdateAsOBB(GetWorldMatrix());
}

void UDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	UpdateDecalMesh();
}

// ==============================================
//				Private Functions 
// ==============================================

void UDecalComponent::HandleFade(float DeltaTime)
{	
	FadeTimer += DeltaTime;

	float Alpha = 1;

	// Fade In
	if (FadeInDuration > 0.0f)
	{
		float InStart = FadeInDelay;
		float InEnd = FadeInDelay + FadeInDuration;
		if (FadeTimer < InStart) 
			Alpha = 0.0f;
		else if (FadeTimer < InEnd) 
			Alpha = (FadeTimer - InStart) / FadeInDuration;
	}

	// Fade Out
	if (FadeOutDuration > 0.0f)
	{
		float OutStart = FadeOutDelay;
		float OutEnd = FadeOutDelay + FadeOutDuration;
		if (FadeTimer > OutEnd) 
			Alpha = 0.0f;
		else if (FadeTimer > OutStart) 
			Alpha = std::min(Alpha, 1.0f - (FadeTimer - OutStart) / FadeOutDuration);
	}

	FadeOpacity = Alpha;
	UE_LOG("Decal Alpha : %f", FadeOpacity);
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::UpdateDecalMesh()
{
	SCOPE_STAT_CAT("UpdateDecalMesh", "6_Decal");
	UpdateOBBFromTransform();

	UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
	if (!World) return;

	TArray<UPrimitiveComponent*> OverlappingPrimitives;
	World->GetPartition().QueryFrustumAllPrimitive(ConvexVolume, OverlappingPrimitives);

	// ===== Calculate Mesh =====
	DecalMeshData.Vertices.clear();
	DecalMeshData.Indices.clear();

	FMatrix InvWorld = GetWorldMatrix().GetInverse();

	for (auto PrimitiveComp : OverlappingPrimitives)
	{
		if (PrimitiveComp == this || PrimitiveComp->GetOwner() == GetOwner()) continue;
		
		// 260411 NOTE
		// 현재로썬 정점 데이터 가져올 방법이 StaticMeshComponent의 GetStaticMesh밖에 없어 
		// 오로지 StaticMeshComponent만 데칼의 대상으로 허용
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(PrimitiveComp);
		if (!SMC) continue;

		UStaticMesh* StaticMeshAsset = SMC->GetStaticMesh();
		if (!StaticMeshAsset) continue;
		
		// Narrow Phase Pruning
		TArray<uint32> TriangleStartIndices;
		
		FOBB BoundBox;
		BoundBox.ApplyTransform(GetWorldMatrix());
		BoundBox.ApplyTransform(SMC->GetWorldMatrix().GetInverse()); // OBB를 Static Mesh의 local 좌표계로 변환
		StaticMeshAsset->GetOBBIntersection(BoundBox, TriangleStartIndices);

		if (TriangleStartIndices.size() == 0) return;

		FMatrix LocalToDecalMatrix = SMC->GetWorldMatrix() * InvWorld;
		TMeshData<FVertexPNCT> Sliced = SutherlandHodgman(
			StaticMeshAsset->GetStaticMeshAsset()->Vertices,
			StaticMeshAsset->GetStaticMeshAsset()->Indices,
			TriangleStartIndices,
			LocalToDecalMatrix);

		if (Sliced.Vertices.empty()) continue;

		uint32 IndexOffset = static_cast<uint32>(DecalMeshData.Vertices.size());
		for (const auto& Vertex : Sliced.Vertices)
			DecalMeshData.Vertices.push_back(Vertex);
		for (auto Index : Sliced.Indices)
			DecalMeshData.Indices.push_back(Index + IndexOffset);
	}

	MarkProxyDirty(EDirtyFlag::Mesh);
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
