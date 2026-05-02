#include "ProceduralMeshComponent.h"
#include "StaticMeshComponent.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UProceduralMeshComponent, UPrimitiveComponent)

void UProceduralMeshComponent::CreateSection(int32 SectionIndex, const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices)
{
    if (Sections.size() <= SectionIndex)
        Sections.resize(SectionIndex + 1);

	Sections[SectionIndex].Vertices = InVertices;
    Sections[SectionIndex].Indices = InIndices;
}

void UProceduralMeshComponent::ClearSection(int32 SectionIndex)
{
	if (Sections.size() > SectionIndex)
    {
        Sections[SectionIndex].Indices.clear();
        Sections[SectionIndex].Vertices.clear();
	}
}

void UProceduralMeshComponent::ClearAllSections()
{
    Sections.clear();
}

void UProceduralMeshComponent::UpdateWorldAABB() const
{
    if (Sections.empty())
        return;

    FVector Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& Section : Sections)
    {
        for (const auto& V : Section.Vertices)
        {
            FVector WorldPos = GetWorldMatrix().TransformPosition(V.Position);
            Min.X = std::min(Min.X, WorldPos.X);
            Min.Y = std::min(Min.Y, WorldPos.Y);
            Min.Z = std::min(Min.Z, WorldPos.Z);

            Max.X = std::max(Max.X, WorldPos.X);
            Max.Y = std::max(Max.Y, WorldPos.Y);
            Max.Z = std::max(Max.Z, WorldPos.Z);
        }
    }

    WorldAABB.Min = Min;
    WorldAABB.Max = Max;
}

bool UProceduralMeshComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UProceduralMeshComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_ProceduralMesh;
}

UMaterialInterface* UProceduralMeshComponent::GetMaterial(int32 SlotIndex) const
{
    if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(Materials.size()))
    {
        return nullptr;
    }

    return Materials[SlotIndex];
}

void UProceduralMeshComponent::SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial)
{
    if (SlotIndex < 0)
    {
        return;
    }

    if (SlotIndex >= static_cast<int32>(Materials.size()))
    {
        Materials.resize(SlotIndex + 1, nullptr);
    }

    if (Materials[SlotIndex] != InMaterial)
    {
        if (UMaterialInstance* MatInst = Cast<UMaterialInstance>(Materials[SlotIndex]))
        {
            delete MatInst;
        }
    }

    Materials[SlotIndex] = InMaterial;
}

void FMeshSlicer::Slice(const FSliceMeshData& InMesh, const FPlane& Plane, FSliceMeshData& OutFront, FSliceMeshData& OutBack)
{
    for (int i = 0; i < InMesh.Indices.size(); i += 3)
    {
		// 삼각형 단위로 처리
        int i0 = InMesh.Indices[i];
        int i1 = InMesh.Indices[i + 1];
        int i2 = InMesh.Indices[i + 2];

        const FNormalVertex& V0 = InMesh.Vertices[i0];
        const FNormalVertex& V1 = InMesh.Vertices[i1];
        const FNormalVertex& V2 = InMesh.Vertices[i2];

        float d0 = Plane.GetSignedDistanceToPoint(V0.Position);
        float d1 = Plane.GetSignedDistanceToPoint(V1.Position);
        float d2 = Plane.GetSignedDistanceToPoint(V2.Position);

		const float EPS = 1e-6f;
        bool b0 = d0 >= -EPS;
        bool b1 = d1 >= -EPS;
        bool b2 = d2 >= -EPS;

		if (b0 && b1 && b2)
        {
			// 전부 Front
            AddTriangle(OutFront, V0, V1, V2);
        }
		else if (!b0 && !b1 && !b2)
		{
			// 전부 Back
            AddTriangle(OutBack, V0, V1, V2);
		}
		else
        {
            FNormalVertex V[3] = { V0, V1, V2 };
            float d[3] = { d0, d1, d2 };
            bool b[3] = { b0, b1, b2 };

			int frontCount = (int)b[0] + (int)b[1] + (int)b[2];

			// 자르는 경우는 Front 1 / Back 2 또는 Front 2 / Back 1
            if (frontCount == 1)
            {
                int f = -1, b1i = -1, b2i = -1;

                for (int k = 0; k < 3; k++)
                {
                    if (b[k])
                        f = k;
                    else if (b1i == -1)
                        b1i = k;
                    else
                        b2i = k;
                }

				if (f == 1)
				{
                    std::swap(b1i, b2i);
				}

                const FNormalVertex& VF = V[f];
                const FNormalVertex& VB1 = V[b1i];
                const FNormalVertex& VB2 = V[b2i];

                float dF = d[f];
                float dB1 = d[b1i];
                float dB2 = d[b2i];

                FNormalVertex I1 = Intersect(VF, VB1, dF, dB1);
                FNormalVertex I2 = Intersect(VF, VB2, dF, dB2);

                // Front
                AddTriangle(OutFront, VF, I1, I2);

                // Back (quad → 2 triangles)
                AddTriangle(OutBack, VB1, VB2, I2);
                AddTriangle(OutBack, VB1, I2, I1);
            }
            else if (frontCount == 2)
            {
                int bIdx = -1, f1 = -1, f2 = -1;

                for (int k = 0; k < 3; k++)
                {
                    if (!b[k])
                        bIdx = k;
                    else if (f1 == -1)
                        f1 = k;
                    else
                        f2 = k;
                }

				if (bIdx == 1)
				{
                    std::swap(f1, f2);
				}

                const FNormalVertex& VB = V[bIdx];
                const FNormalVertex& VF1 = V[f1];
                const FNormalVertex& VF2 = V[f2];

                float dB = d[bIdx];
                float dF1 = d[f1];
                float dF2 = d[f2];

                FNormalVertex I1 = Intersect(VF1, VB, dF1, dB);
                FNormalVertex I2 = Intersect(VF2, VB, dF2, dB);

                // Back
                AddTriangle(OutBack, VB, I1, I2);

                // Front (quad → 2 triangles)
                AddTriangle(OutFront, VF1, VF2, I2);
                AddTriangle(OutFront, VF1, I2, I1);
            }
			else
			{
                assert(false && "Unknown Case");
			}
		}

    }
}

void FMeshSlicer::SliceComponent(UPrimitiveComponent* InComponent, const FPlane& Plane, UProceduralMeshComponent*& OutFront, UProceduralMeshComponent*& OutBack)
{
    // 데이터 복사
    UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(InComponent);

    if (StaticMeshComp)
    {
        FSliceMeshData Src;
        StaticMeshComp->GetMeshData(Src.Vertices, Src.Indices);

        FSliceMeshData Front;
        FSliceMeshData Back;

        Slice(Src, Plane, Front, Back);

        // ProceduralMesh 생성
        OutFront->CreateSection(0, Front.Vertices, Front.Indices);
		// 테스트용
        OutFront->SetWorldLocation(FVector(0, 0, 1));

		for (int32 i = 0; i < InComponent->GetNumMaterials(); i++)
        {
            UMaterialInterface* Mat = InComponent->GetMaterial(i);
            OutFront->SetMaterial(i, InComponent->GetMaterial(i));
		}

        OutBack->CreateSection(0, Back.Vertices, Back.Indices);
        OutBack->SetWorldLocation(FVector(0, 0, -1));
        for (int32 i = 0; i < InComponent->GetNumMaterials(); i++)
        {
            OutBack->SetMaterial(i, InComponent->GetMaterial(i));
        }

		// 기존 Static Mesh 제거 or 숨김
    }
}

void FMeshSlicer::AddTriangle(FSliceMeshData& Mesh, const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C)
{
	// Vertex 중복은 많은데, 우선 돌아가게 하기 위해서 단순한 방식으로 채택
    int Base = Mesh.Vertices.size();

	Mesh.Vertices.push_back(A);
    Mesh.Vertices.push_back(B);
    Mesh.Vertices.push_back(C);

	Mesh.Indices.push_back(Base);
    Mesh.Indices.push_back(Base + 1);
    Mesh.Indices.push_back(Base + 2);
}

FNormalVertex FMeshSlicer::Intersect(const FNormalVertex& A, const FNormalVertex& B, float dA, float dB)
{
    float denom = (dA - dB);
    if (fabs(denom) < 1e-8f)
    {
        return A; // fallback (아무거나 한쪽)
    }
    float t = dA / denom;

    FNormalVertex R;
    R.Position = A.Position + (B.Position - A.Position) * t;
    R.Normal = (A.Normal + (B.Normal - A.Normal) * t).GetSafeNormal();
    return R;
}
