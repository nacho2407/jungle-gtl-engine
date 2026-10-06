#include "Render/Proxy/ClothSceneProxy.h"

#include "Cloth/ClothTypes.h"
#include "Component/Primitive/ClothComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Profiling/Stats/ClothStats.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Geometry/CollisionDebugGeometry.h"
#include "Render/Geometry/DebugGeometryTypes.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float GClothDebugEpsilon = 1.0e-4f;
	constexpr float GClothPinnedMarkerSize = 0.025f;
	constexpr float GClothPlaneDebugSizeMin = 25.0f;
	constexpr float GClothVectorArrowHeadSize = 8.0f;
	constexpr float GClothWindVectorScale = 0.25f;
	constexpr float GClothOwnerMotionVectorScale = 5.0f;
	constexpr float GClothMaxVectorDebugLength = 200.0f;

	FVector4 GetCollisionDebugColor(EClothCollisionPrimitiveSource Source)
	{
		switch (Source)
		{
		case EClothCollisionPrimitiveSource::Independent:
			return FVector4(0.0f, 0.85f, 1.0f, 1.0f);
		case EClothCollisionPrimitiveSource::Body:
			return FVector4(1.0f, 0.45f, 0.1f, 1.0f);
		default:
			return FVector4(0.75f, 0.75f, 0.75f, 1.0f);
		}
	}

	void AddDebugLine(
		TArray<FPhysicsDebugLine>& OutLines,
		const FVector& Start,
		const FVector& End,
		const FVector4& Color)
	{
		if ((End - Start).Length() <= GClothDebugEpsilon)
		{
			return;
		}

		OutLines.push_back({ Start, End, Color });
	}

	void AppendWireLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const TArray<FWireLine>& WireLines,
		const FVector4& Color)
	{
		for (const FWireLine& Line : WireLines)
		{
			AddDebugLine(OutLines, Line.Start, Line.End, Color);
		}
	}

	float TransformLengthConservative(const FMatrix& Matrix, float LocalLength)
	{
		const float SafeLength = (std::max)(0.0f, LocalLength);
		if (SafeLength <= GClothDebugEpsilon)
		{
			return 0.0f;
		}

		const float LengthX = Matrix.TransformVector(FVector(SafeLength, 0.0f, 0.0f)).Length();
		const float LengthY = Matrix.TransformVector(FVector(0.0f, SafeLength, 0.0f)).Length();
		const float LengthZ = Matrix.TransformVector(FVector(0.0f, 0.0f, SafeLength)).Length();
		return (std::max)(LengthX, (std::max)(LengthY, LengthZ));
	}

	void BuildPerpendicularBasis(const FVector& Axis, FVector& OutAxisX, FVector& OutAxisY)
	{
		const FVector SafeAxis = Axis.GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector);
		const FVector Reference =
			std::fabs(SafeAxis.Z) < 0.8f ? FVector::ZAxisVector : FVector::YAxisVector;

		OutAxisX = Reference.Cross(SafeAxis).GetSafeNormal(GClothDebugEpsilon, FVector::XAxisVector);
		OutAxisY = SafeAxis.Cross(OutAxisX).GetSafeNormal(GClothDebugEpsilon, FVector::YAxisVector);
	}

	void AddAabbLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FBoundingBox& Bounds,
		const FVector4& Color)
	{
		const FVector Corners[8] =
		{
			FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z),
			FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Min.Z),
			FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Min.Z),
			FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Min.Z),
			FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z),
			FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Z),
			FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Max.Z),
			FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z)
		};

		AddDebugLine(OutLines, Corners[0], Corners[1], Color);
		AddDebugLine(OutLines, Corners[1], Corners[3], Color);
		AddDebugLine(OutLines, Corners[3], Corners[2], Color);
		AddDebugLine(OutLines, Corners[2], Corners[0], Color);
		AddDebugLine(OutLines, Corners[4], Corners[5], Color);
		AddDebugLine(OutLines, Corners[5], Corners[7], Color);
		AddDebugLine(OutLines, Corners[7], Corners[6], Color);
		AddDebugLine(OutLines, Corners[6], Corners[4], Color);
		AddDebugLine(OutLines, Corners[0], Corners[4], Color);
		AddDebugLine(OutLines, Corners[1], Corners[5], Color);
		AddDebugLine(OutLines, Corners[2], Corners[6], Color);
		AddDebugLine(OutLines, Corners[3], Corners[7], Color);
	}

	void AddBoxLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FVector& Center,
		const FVector& AxisX,
		const FVector& AxisY,
		const FVector& AxisZ,
		const FVector4& Color)
	{
		const FVector Corners[8] =
		{
			Center - AxisX - AxisY - AxisZ,
			Center + AxisX - AxisY - AxisZ,
			Center - AxisX + AxisY - AxisZ,
			Center + AxisX + AxisY - AxisZ,
			Center - AxisX - AxisY + AxisZ,
			Center + AxisX - AxisY + AxisZ,
			Center - AxisX + AxisY + AxisZ,
			Center + AxisX + AxisY + AxisZ
		};

		AddDebugLine(OutLines, Corners[0], Corners[1], Color);
		AddDebugLine(OutLines, Corners[1], Corners[3], Color);
		AddDebugLine(OutLines, Corners[3], Corners[2], Color);
		AddDebugLine(OutLines, Corners[2], Corners[0], Color);
		AddDebugLine(OutLines, Corners[4], Corners[5], Color);
		AddDebugLine(OutLines, Corners[5], Corners[7], Color);
		AddDebugLine(OutLines, Corners[7], Corners[6], Color);
		AddDebugLine(OutLines, Corners[6], Corners[4], Color);
		AddDebugLine(OutLines, Corners[0], Corners[4], Color);
		AddDebugLine(OutLines, Corners[1], Corners[5], Color);
		AddDebugLine(OutLines, Corners[2], Corners[6], Color);
		AddDebugLine(OutLines, Corners[3], Corners[7], Color);
	}

	void AddParticleMarker(
		TArray<FPhysicsDebugLine>& OutLines,
		const FVector& Position,
		const FVector4& Color)
	{
		// particle 위치 확인용 작은 wire sphere
		TArray<FWireLine> WireLines;
		FCollisionDebugGeometry::AddWireSphere(WireLines, Position, GClothPinnedMarkerSize);
		AppendWireLines(OutLines, WireLines, Color);
	}

	/**
	 * @brief component local rect debug shape를 world line으로 추가합니다
	 */
	void AddRectLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FVector& Center,
		const FVector& AxisX,
		const FVector& AxisZ,
		const FVector4& Color)
	{
		const FVector Corner0 = Center - AxisX - AxisZ;
		const FVector Corner1 = Center + AxisX - AxisZ;
		const FVector Corner2 = Center + AxisX + AxisZ;
		const FVector Corner3 = Center - AxisX + AxisZ;

		AddDebugLine(OutLines, Corner0, Corner1, Color);
		AddDebugLine(OutLines, Corner1, Corner2, Color);
		AddDebugLine(OutLines, Corner2, Corner3, Color);
		AddDebugLine(OutLines, Corner3, Corner0, Color);
	}

	/**
	 * @brief actor local pin 선택 영역 debug shape를 world line으로 추가합니다
	 */
	void AddPinSelectionDebugShapeLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FMatrix& WorldMatrix,
		const FClothPinSelectionDebugShape& Shape,
		const FVector4& Color)
	{
		switch (Shape.Type)
		{
		case EClothPinSelectionDebugShapeType::Sphere:
		{
			// pin 선택 sphere 영역 표시
			TArray<FWireLine> WireLines;
			const FVector WorldCenter = WorldMatrix.TransformPositionWithW(Shape.Center);
			const float WorldRadius = TransformLengthConservative(WorldMatrix, Shape.Radius);
			FCollisionDebugGeometry::AddWireSphere(WireLines, WorldCenter, WorldRadius);
			AppendWireLines(OutLines, WireLines, Color);
			break;
		}

		case EClothPinSelectionDebugShapeType::Box:
		{
			// pin 선택 box 영역 표시
			const FVector Center = WorldMatrix.TransformPositionWithW(Shape.Center);
			const FVector AxisX = WorldMatrix.TransformVector(
				Shape.AxisX.GetSafeNormal(GClothDebugEpsilon, FVector::XAxisVector) * Shape.Extent.X);
			const FVector AxisY = WorldMatrix.TransformVector(
				Shape.AxisY.GetSafeNormal(GClothDebugEpsilon, FVector::YAxisVector) * Shape.Extent.Y);
			const FVector AxisZ = WorldMatrix.TransformVector(
				Shape.AxisZ.GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector) * Shape.Extent.Z);
			AddBoxLines(OutLines, Center, AxisX, AxisY, AxisZ, Color);
			break;
		}

		case EClothPinSelectionDebugShapeType::RectXZ:
		{
			// pin 선택 xz rectangle 영역 표시
			const FVector Center = WorldMatrix.TransformPositionWithW(Shape.Center);
			const FVector AxisX = WorldMatrix.TransformVector(
				Shape.AxisX.GetSafeNormal(GClothDebugEpsilon, FVector::XAxisVector) * Shape.Extent.X);
			const FVector AxisZ = WorldMatrix.TransformVector(
				Shape.AxisZ.GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector) * Shape.Extent.Z);
			AddRectLines(OutLines, Center, AxisX, AxisZ, Color);
			break;
		}

		default:
			break;
		}
	}

	void AddVectorArrow(
		TArray<FPhysicsDebugLine>& OutLines,
		const FVector& Origin,
		const FVector& Vector,
		float Scale,
		const FVector4& Color)
	{
		const float VectorLength = Vector.Length();
		if (VectorLength <= GClothDebugEpsilon)
		{
			return;
		}

		const FVector Direction = Vector * (1.0f / VectorLength);
		const float DebugLength = (std::min)(VectorLength * Scale, GClothMaxVectorDebugLength);
		const FVector End = Origin + Direction * DebugLength;
		AddDebugLine(OutLines, Origin, End, Color);

		FVector ArrowAxisX;
		FVector ArrowAxisY;
		BuildPerpendicularBasis(Direction, ArrowAxisX, ArrowAxisY);
		const float ArrowSize = (std::min)(GClothVectorArrowHeadSize, DebugLength * 0.25f);
		AddDebugLine(OutLines, End, End - Direction * ArrowSize + ArrowAxisX * (ArrowSize * 0.5f), Color);
		AddDebugLine(OutLines, End, End - Direction * ArrowSize - ArrowAxisX * (ArrowSize * 0.5f), Color);
		AddDebugLine(OutLines, End, End - Direction * ArrowSize + ArrowAxisY * (ArrowSize * 0.5f), Color);
		AddDebugLine(OutLines, End, End - Direction * ArrowSize - ArrowAxisY * (ArrowSize * 0.5f), Color);
	}

	void AddSpherePrimitiveLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FMatrix& WorldMatrix,
		const FClothCollisionPrimitive& Primitive,
		const FVector4& Color)
	{
		TArray<FWireLine> WireLines;
		const FVector WorldCenter = WorldMatrix.TransformPositionWithW(Primitive.Center);
		const float WorldRadius = TransformLengthConservative(WorldMatrix, Primitive.Radius);
		FCollisionDebugGeometry::AddWireSphere(WireLines, WorldCenter, WorldRadius);
		AppendWireLines(OutLines, WireLines, Color);
	}

	void AddCapsulePrimitiveLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FMatrix& WorldMatrix,
		const FClothCollisionPrimitive& Primitive,
		const FVector4& Color)
	{
		TArray<FWireLine> WireLines;
		const FVector WorldStart = WorldMatrix.TransformPositionWithW(Primitive.CapsuleStart);
		const FVector WorldEnd = WorldMatrix.TransformPositionWithW(Primitive.CapsuleEnd);
		const float WorldRadius = TransformLengthConservative(WorldMatrix, Primitive.Radius);
		const FVector Axis = (WorldEnd - WorldStart).GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector);

		FCollisionDebugGeometry::AddWireSphere(WireLines, WorldStart, WorldRadius);
		FCollisionDebugGeometry::AddWireSphere(WireLines, WorldEnd, WorldRadius);
		AppendWireLines(OutLines, WireLines, Color);

		FVector AxisX;
		FVector AxisY;
		BuildPerpendicularBasis(Axis, AxisX, AxisY);
		AddDebugLine(OutLines, WorldStart + AxisX * WorldRadius, WorldEnd + AxisX * WorldRadius, Color);
		AddDebugLine(OutLines, WorldStart - AxisX * WorldRadius, WorldEnd - AxisX * WorldRadius, Color);
		AddDebugLine(OutLines, WorldStart + AxisY * WorldRadius, WorldEnd + AxisY * WorldRadius, Color);
		AddDebugLine(OutLines, WorldStart - AxisY * WorldRadius, WorldEnd - AxisY * WorldRadius, Color);
	}

	void AddBoxPrimitiveLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FMatrix& WorldMatrix,
		const FClothCollisionPrimitive& Primitive,
		const FVector4& Color)
	{
		const FVector Center = WorldMatrix.TransformPositionWithW(Primitive.Center);
		const FVector AxisX = WorldMatrix.TransformVector(
			Primitive.BoxAxisX.GetSafeNormal(GClothDebugEpsilon, FVector::XAxisVector) * Primitive.BoxExtent.X);
		const FVector AxisY = WorldMatrix.TransformVector(
			Primitive.BoxAxisY.GetSafeNormal(GClothDebugEpsilon, FVector::YAxisVector) * Primitive.BoxExtent.Y);
		const FVector AxisZ = WorldMatrix.TransformVector(
			Primitive.BoxAxisZ.GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector) * Primitive.BoxExtent.Z);

		AddBoxLines(OutLines, Center, AxisX, AxisY, AxisZ, Color);
	}

	void AddPlanePrimitiveLines(
		TArray<FPhysicsDebugLine>& OutLines,
		const FMatrix& WorldMatrix,
		const FBoundingBox& Bounds,
		const FClothCollisionPrimitive& Primitive,
		const FVector4& Color)
	{
		const FVector WorldPoint = WorldMatrix.TransformPositionWithW(Primitive.PlanePoint);
		const FVector WorldNormal = WorldMatrix.TransformVector(Primitive.PlaneNormal)
			.GetSafeNormal(GClothDebugEpsilon, FVector::ZAxisVector);
		FVector AxisX;
		FVector AxisY;
		BuildPerpendicularBasis(WorldNormal, AxisX, AxisY);

		const float PlaneSize = (std::max)(GClothPlaneDebugSizeMin, Bounds.GetExtent().Length() * 0.35f);
		const FVector P0 = WorldPoint - AxisX * PlaneSize - AxisY * PlaneSize;
		const FVector P1 = WorldPoint + AxisX * PlaneSize - AxisY * PlaneSize;
		const FVector P2 = WorldPoint + AxisX * PlaneSize + AxisY * PlaneSize;
		const FVector P3 = WorldPoint - AxisX * PlaneSize + AxisY * PlaneSize;

		AddDebugLine(OutLines, P0, P1, Color);
		AddDebugLine(OutLines, P1, P2, Color);
		AddDebugLine(OutLines, P2, P3, Color);
		AddDebugLine(OutLines, P3, P0, Color);
		AddDebugLine(OutLines, WorldPoint, WorldPoint + WorldNormal * (PlaneSize * 0.5f), Color);
	}
}

FClothSceneProxy::FClothSceneProxy(UClothComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::Cloth;
}

UClothComponent* FClothSceneProxy::GetClothComponent() const
{
	return Cast<UClothComponent>(GetOwner());
}

void FClothSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FClothSceneProxy::UpdateMesh()
{
	if (!HasValidOwner())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		CachedMaterial = nullptr;
		CachedVertexCount = 0;
		CachedIndexCount = 0;
		bVisible = false;
		return;
	}

	// Cloth는 FMeshBuffer 대신 component의 CPU render data와 proxy dynamic buffer를 사용
	MeshBuffer = nullptr;

	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		SectionDraws.clear();
		CachedMaterial = nullptr;
		CachedVertexCount = 0;
		CachedIndexCount = 0;
		bVisible = false;
		return;
	}

	// 초기 등록 또는 property dirty 직후 render data가 비어 있으면 한 번 생성 보장
	ClothComponent->RebuildClothIfNeeded(false);

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	CachedVertexCount = static_cast<uint32>(RenderData.Vertices.size());
	CachedIndexCount = static_cast<uint32>(RenderData.Indices.size());
	UploadedRevision = 0;
	bDynamicBuffersNeedCreate = true;

	RebuildSectionDraws();
}

bool FClothSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		return false;
	}

	// editor 초기 표시 경로에서 tick보다 draw 준비가 먼저 오는 경우 방어
	ClothComponent->RebuildClothIfNeeded(false);

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	const uint32 VertexCount = static_cast<uint32>(RenderData.Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(RenderData.Indices.size());
	if (!RenderData.IsValid() || VertexCount == 0 || IndexCount == 0)
	{
		return false;
	}

	if (bDynamicBuffersNeedCreate || !DynamicVertexBuffer.GetBuffer() || !DynamicIndexBuffer.GetBuffer())
	{
		const uint32 InitialVertexCapacity = (std::max)(CachedVertexCount, VertexCount);
		const uint32 InitialIndexCapacity = (std::max)(CachedIndexCount, IndexCount);
		DynamicVertexBuffer.Create(Device, InitialVertexCapacity, sizeof(FVertexPNCTT));
		DynamicIndexBuffer.Create(Device, InitialIndexCapacity);
		UploadedRevision = 0;
		bDynamicBuffersNeedCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);
	DynamicIndexBuffer.EnsureCapacity(Device, IndexCount);

	const uint64 CurrentRevision = RenderData.Revision;
	if (UploadedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, RenderData.Vertices.data(), VertexCount))
		{
			return false;
		}

		if (!DynamicIndexBuffer.Update(Context, RenderData.Indices.data(), IndexCount))
		{
			return false;
		}

		// revision 변경으로 실제 vertex upload가 성공한 frame만 stat에 반영
		CLOTH_STATS_ADD_VERTEX_UPLOAD();
		UploadedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = DynamicIndexBuffer.GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

void FClothSceneProxy::BuildClothDebugLines(const FFrameContext& Frame, TArray<FPhysicsDebugLine>& OutLines) const
{
	(void)Frame;

	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		return;
	}

	// debug line 생성 경로에서는 component 상태를 변경하지 않고 현재 snapshot만 읽음
	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	if (!RenderData.IsValid())
	{
		return;
	}

	const FMatrix& WorldMatrix = ClothComponent->GetWorldMatrix();
	const FBoundingBox& Bounds = GetCachedBounds();
	const FVector BoundsCenter = Bounds.GetCenter();

	const FVector4 BoundsColor(1.0f, 1.0f, 1.0f, 0.9f);
	const FVector4 ParticleColor(1.0f, 0.95f, 0.1f, 1.0f);
	const FVector4 PinnedColor(1.0f, 0.05f, 0.05f, 1.0f);
	const FVector4 PinSelectionColor(0.45f, 1.0f, 0.2f, 1.0f);
	const FVector4 WindColor(0.2f, 0.45f, 1.0f, 1.0f);
	const FVector4 OwnerMotionColor(1.0f, 0.15f, 1.0f, 1.0f);

	// cloth simulation snapshot에 들어간 collision primitive 표시
	for (const FClothCollisionPrimitive& Primitive : ClothComponent->GetCachedCollisionPrimitives())
	{
		const FVector4 CollisionColor = GetCollisionDebugColor(Primitive.Source);
		switch (Primitive.Type)
		{
		case EClothCollisionPrimitiveType::Sphere:
			AddSpherePrimitiveLines(OutLines, WorldMatrix, Primitive, CollisionColor);
			break;
		case EClothCollisionPrimitiveType::Capsule:
			AddCapsulePrimitiveLines(OutLines, WorldMatrix, Primitive, CollisionColor);
			break;
		case EClothCollisionPrimitiveType::Box:
			AddBoxPrimitiveLines(OutLines, WorldMatrix, Primitive, CollisionColor);
			break;
		case EClothCollisionPrimitiveType::Plane:
			AddPlanePrimitiveLines(OutLines, WorldMatrix, Bounds, Primitive, CollisionColor);
			break;
		}
	}

	// actor local pin 선택 영역 표시
	FClothPinSelectionDebugShape PinSelectionShape;
	if (ClothComponent->BuildPinSelectionDebugShape(PinSelectionShape))
	{
		AddPinSelectionDebugShapeLines(OutLines, WorldMatrix, PinSelectionShape, PinSelectionColor);
	}

	// 전체 cloth particle 위치를 노란 wire sphere로 표시
	for (const auto& Vertex : RenderData.Vertices)
	{
		const FVector WorldPosition = WorldMatrix.TransformPositionWithW(Vertex.Position);
		AddParticleMarker(OutLines, WorldPosition, ParticleColor);
	}

	// hard pin particle 위치를 빨간 wire sphere로 강조 표시
	for (uint32 PinnedIndex : ClothComponent->GetCachedPinnedIndices())
	{
		if (PinnedIndex >= RenderData.Vertices.size())
		{
			continue;
		}

		const FVector WorldPosition = WorldMatrix.TransformPositionWithW(RenderData.Vertices[PinnedIndex].Position);
		AddParticleMarker(OutLines, WorldPosition, PinnedColor);
	}

	// bounds와 milestone 3 vector cache 표시
	AddAabbLines(OutLines, Bounds, BoundsColor);
	AddVectorArrow(OutLines, BoundsCenter, ClothComponent->GetCachedFinalWindVelocityWorld(), GClothWindVectorScale, WindColor);
	AddVectorArrow(OutLines, BoundsCenter, ClothComponent->GetCachedOwnerMotionDeltaWorld(), GClothOwnerMotionVectorScale, OwnerMotionColor);
}

void FClothSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	FPrimitiveSceneProxy::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(CachedMaterial, "FClothSceneProxy.CachedMaterial");
}

void FClothSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();
	CachedMaterial = nullptr;

	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		MeshBuffer = nullptr;
		return;
	}

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	if (!RenderData.IsValid())
	{
		return;
	}

	CachedMaterial = ClothComponent->GetMaterial(0);
	if (!CachedMaterial)
	{
		CachedMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");
	}

	SectionDraws.reserve(RenderData.Sections.size());
	for (const FClothRenderSection& Section : RenderData.Sections)
	{
		if (Section.IndexCount == 0)
		{
			continue;
		}

		FMeshSectionDraw Draw;
		Draw.Material = CachedMaterial;
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.IndexCount;
		SectionDraws.push_back(Draw);
	}
}
