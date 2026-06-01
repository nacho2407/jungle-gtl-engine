#include "StaticMeshEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Core/Logging/Log.h"
#include "Math/MathUtils.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/MeshManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "UI/Util/InlinePropertyRenderer.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace
{
	constexpr float ConvexPointWeldTolerance = 0.001f;
	constexpr float ConvexPlaneToleranceScale = 0.0001f;
	constexpr float ConvexMinimumExtent = 0.5f;

	struct FConvexPointKey
	{
		long long X = 0;
		long long Y = 0;
		long long Z = 0;

		bool operator==(const FConvexPointKey& Other) const
		{
			return X == Other.X && Y == Other.Y && Z == Other.Z;
		}
	};

	struct FConvexPointKeyHash
	{
		size_t operator()(const FConvexPointKey& Key) const
		{
			const size_t XHash = std::hash<long long>()(Key.X);
			const size_t YHash = std::hash<long long>()(Key.Y);
			const size_t ZHash = std::hash<long long>()(Key.Z);
			return XHash ^ (YHash + 0x9e3779b97f4a7c15ull + (XHash << 6) + (XHash >> 2))
				^ (ZHash + 0x9e3779b97f4a7c15ull + (YHash << 6) + (YHash >> 2));
		}
	};

	struct FConvexHullFace
	{
		int32 A = -1;
		int32 B = -1;
		int32 C = -1;
		bool bValid = true;
	};

	struct FConvexHullEdge
	{
		int32 A = -1;
		int32 B = -1;
	};

	FString FormatStaticMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}

	FConvexPointKey MakeConvexPointKey(const FVector& Point)
	{
		return {
			static_cast<long long>(std::llround(Point.X / ConvexPointWeldTolerance)),
			static_cast<long long>(std::llround(Point.Y / ConvexPointWeldTolerance)),
			static_cast<long long>(std::llround(Point.Z / ConvexPointWeldTolerance)),
		};
	}

	float GetDistanceSquaredToLine(const FVector& Point, const FVector& LineStart, const FVector& LineEnd)
	{
		const FVector Line = LineEnd - LineStart;
		const float LineLengthSq = Line.Dot(Line);
		if (LineLengthSq <= FMath::KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		return (Point - LineStart).Cross(Line).Dot((Point - LineStart).Cross(Line)) / LineLengthSq;
	}

	float GetSignedDistanceToPlane(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector Normal = (B - A).Cross(C - A);
		const float NormalLength = Normal.Length();
		if (NormalLength <= FMath::KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		return Normal.Dot(Point - A) / NormalLength;
	}

	void OrientConvexHullFace(FConvexHullFace& Face, const TArray<FVector>& Points, const FVector& InteriorPoint)
	{
		const FVector Normal = (Points[Face.B] - Points[Face.A]).Cross(Points[Face.C] - Points[Face.A]);
		if (Normal.Dot(InteriorPoint - Points[Face.A]) > 0.0f)
		{
			const int32 Temp = Face.B;
			Face.B = Face.C;
			Face.C = Temp;
		}
	}

	bool IsConvexHullFaceVisible(const FConvexHullFace& Face, const TArray<FVector>& Points, const FVector& Point, float Tolerance)
	{
		const FVector Normal = (Points[Face.B] - Points[Face.A]).Cross(Points[Face.C] - Points[Face.A]);
		const float NormalLength = Normal.Length();
		if (NormalLength <= FMath::KINDA_SMALL_NUMBER)
		{
			return false;
		}

		return Normal.Dot(Point - Points[Face.A]) / NormalLength > Tolerance;
	}

	void AddBoundaryEdge(TArray<FConvexHullEdge>& BoundaryEdges, int32 A, int32 B)
	{
		for (auto It = BoundaryEdges.begin(); It != BoundaryEdges.end(); ++It)
		{
			if ((It->A == B && It->B == A) || (It->A == A && It->B == B))
			{
				*It = BoundaryEdges.back();
				BoundaryEdges.pop_back();
				return;
			}
		}

		BoundaryEdges.push_back({ A, B });
	}

	FKConvexElem MakeFallbackConvexElem(FStaticMesh* MeshAsset)
	{
		FVector Center = FVector::ZeroVector;
		FVector Extent(ConvexMinimumExtent, ConvexMinimumExtent, ConvexMinimumExtent);
		if (MeshAsset && !MeshAsset->Vertices.empty())
		{
			if (!MeshAsset->bBoundsValid)
			{
				MeshAsset->CacheBounds();
			}
			if (MeshAsset->bBoundsValid)
			{
				Center = MeshAsset->BoundsCenter;
				Extent = FVector(
					FMath::Max(MeshAsset->BoundsExtent.X, ConvexMinimumExtent),
					FMath::Max(MeshAsset->BoundsExtent.Y, ConvexMinimumExtent),
					FMath::Max(MeshAsset->BoundsExtent.Z, ConvexMinimumExtent));
			}
		}

		FKConvexElem ConvexElem;
		ConvexElem.VertexData = {
			Center + FVector(-Extent.X, -Extent.Y, -Extent.Z),
			Center + FVector(Extent.X, -Extent.Y, -Extent.Z),
			Center + FVector(Extent.X, Extent.Y, -Extent.Z),
			Center + FVector(-Extent.X, Extent.Y, -Extent.Z),
			Center + FVector(-Extent.X, -Extent.Y, Extent.Z),
			Center + FVector(Extent.X, -Extent.Y, Extent.Z),
			Center + FVector(Extent.X, Extent.Y, Extent.Z),
			Center + FVector(-Extent.X, Extent.Y, Extent.Z),
		};
		ConvexElem.IndexData = {
			0, 2, 1, 0, 3, 2,
			4, 5, 6, 4, 6, 7,
			0, 1, 5, 0, 5, 4,
			1, 2, 6, 1, 6, 5,
			2, 3, 7, 2, 7, 6,
			3, 0, 4, 3, 4, 7,
		};
		ConvexElem.UpdateElemBox();
		return ConvexElem;
	}

	bool BuildConvexHullFromStaticMesh(FStaticMesh* MeshAsset, FKConvexElem& OutConvexElem)
	{
		if (!MeshAsset || MeshAsset->Vertices.size() < 4)
		{
			return false;
		}

		TArray<FVector> Points;
		Points.reserve(MeshAsset->Vertices.size());
		std::unordered_map<FConvexPointKey, int32, FConvexPointKeyHash> WeldedPointIndices;
		WeldedPointIndices.reserve(MeshAsset->Vertices.size());
		for (const FNormalVertex& Vertex : MeshAsset->Vertices)
		{
			const FConvexPointKey Key = MakeConvexPointKey(Vertex.pos);
			if (WeldedPointIndices.find(Key) != WeldedPointIndices.end())
			{
				continue;
			}

			WeldedPointIndices.emplace(Key, static_cast<int32>(Points.size()));
			Points.push_back(Vertex.pos);
		}

		if (Points.size() < 4)
		{
			return false;
		}

		int32 I0 = 0;
		int32 I1 = 0;
		for (int32 Index = 1; Index < static_cast<int32>(Points.size()); ++Index)
		{
			if (Points[Index].X < Points[I0].X)
			{
				I0 = Index;
			}
			if (Points[Index].X > Points[I1].X)
			{
				I1 = Index;
			}
		}

		if (I0 == I1 || FVector::DistSquared(Points[I0], Points[I1]) <= FMath::KINDA_SMALL_NUMBER)
		{
			return false;
		}

		int32 I2 = -1;
		float BestLineDistanceSq = 0.0f;
		for (int32 Index = 0; Index < static_cast<int32>(Points.size()); ++Index)
		{
			if (Index == I0 || Index == I1)
			{
				continue;
			}

			const float DistanceSq = GetDistanceSquaredToLine(Points[Index], Points[I0], Points[I1]);
			if (DistanceSq > BestLineDistanceSq)
			{
				BestLineDistanceSq = DistanceSq;
				I2 = Index;
			}
		}

		if (I2 < 0 || BestLineDistanceSq <= FMath::KINDA_SMALL_NUMBER)
		{
			return false;
		}

		int32 I3 = -1;
		float BestPlaneDistance = 0.0f;
		for (int32 Index = 0; Index < static_cast<int32>(Points.size()); ++Index)
		{
			if (Index == I0 || Index == I1 || Index == I2)
			{
				continue;
			}

			const float Distance = FMath::Abs(GetSignedDistanceToPlane(Points[Index], Points[I0], Points[I1], Points[I2]));
			if (Distance > BestPlaneDistance)
			{
				BestPlaneDistance = Distance;
				I3 = Index;
			}
		}

		if (I3 < 0 || BestPlaneDistance <= FMath::KINDA_SMALL_NUMBER)
		{
			return false;
		}

		if (!MeshAsset->bBoundsValid)
		{
			MeshAsset->CacheBounds();
		}
		const float BoundsScale = MeshAsset->bBoundsValid ? MeshAsset->BoundsExtent.GetAbsMax() : 1.0f;
		const float PlaneTolerance = FMath::Max(BoundsScale * ConvexPlaneToleranceScale, ConvexPointWeldTolerance);
		const FVector InteriorPoint = (Points[I0] + Points[I1] + Points[I2] + Points[I3]) * 0.25f;

		TArray<FConvexHullFace> Faces = {
			{ I0, I1, I2, true },
			{ I0, I3, I1, true },
			{ I0, I2, I3, true },
			{ I1, I3, I2, true },
		};
		for (FConvexHullFace& Face : Faces)
		{
			OrientConvexHullFace(Face, Points, InteriorPoint);
		}

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
		{
			if (PointIndex == I0 || PointIndex == I1 || PointIndex == I2 || PointIndex == I3)
			{
				continue;
			}

			TArray<int32> VisibleFaces;
			for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(Faces.size()); ++FaceIndex)
			{
				if (Faces[FaceIndex].bValid && IsConvexHullFaceVisible(Faces[FaceIndex], Points, Points[PointIndex], PlaneTolerance))
				{
					VisibleFaces.push_back(FaceIndex);
				}
			}

			if (VisibleFaces.empty())
			{
				continue;
			}

			TArray<FConvexHullEdge> BoundaryEdges;
			for (int32 FaceIndex : VisibleFaces)
			{
				const FConvexHullFace& Face = Faces[FaceIndex];
				AddBoundaryEdge(BoundaryEdges, Face.A, Face.B);
				AddBoundaryEdge(BoundaryEdges, Face.B, Face.C);
				AddBoundaryEdge(BoundaryEdges, Face.C, Face.A);
			}

			for (int32 FaceIndex : VisibleFaces)
			{
				Faces[FaceIndex].bValid = false;
			}

			for (const FConvexHullEdge& Edge : BoundaryEdges)
			{
				FConvexHullFace NewFace { Edge.A, Edge.B, PointIndex, true };
				OrientConvexHullFace(NewFace, Points, InteriorPoint);
				Faces.push_back(NewFace);
			}
		}

		TArray<int32> IndexData;
		for (const FConvexHullFace& Face : Faces)
		{
			if (!Face.bValid)
			{
				continue;
			}
			IndexData.push_back(Face.A);
			IndexData.push_back(Face.B);
			IndexData.push_back(Face.C);
		}

		if (IndexData.size() < 12)
		{
			return false;
		}

		TArray<FVector> SurfacePoints;
		TArray<int32> PointRemap;
		PointRemap.resize(Points.size(), -1);
		for (int32& Index : IndexData)
		{
			if (Index < 0 || static_cast<size_t>(Index) >= Points.size())
			{
				return false;
			}

			if (PointRemap[Index] < 0)
			{
				PointRemap[Index] = static_cast<int32>(SurfacePoints.size());
				SurfacePoints.push_back(Points[Index]);
			}
			Index = PointRemap[Index];
		}

		OutConvexElem = FKConvexElem();
		OutConvexElem.VertexData = std::move(SurfacePoints);
		OutConvexElem.IndexData = std::move(IndexData);
		OutConvexElem.UpdateElemBox();
		return true;
	}
}

static uint32 GNextStaticMeshEditorInstanceId = 0;

FStaticMeshEditorWidget::FStaticMeshEditorWidget()
	: InstanceId(GNextStaticMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("StaticMeshEditorPreview_" + Id);
	WindowIdSuffix = "###StaticMeshEditor_" + Id;
}

bool FStaticMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UStaticMesh>();
}

bool FStaticMeshEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UStaticMesh* CurrentMesh = Cast<UStaticMesh>(EditedObject);
	const UStaticMesh* RequestedMesh = Cast<UStaticMesh>(Object);
	if (!IsOpen() || !CurrentMesh || !RequestedMesh)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMesh->GetAssetPathFileName();
}

void FStaticMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(EditedObject))
	{
		UStaticMeshComponent* Comp = Actor->AddComponent<UStaticMeshComponent>();
		Comp->SetStaticMesh(Mesh);
		Actor->SetRootComponent(Comp);
	}
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<UStaticMeshComponent>());
	ViewportClient.CreatePreviewGizmo();
	ViewportClient.ResetCameraToPreviewBounds();
	ViewportClient.SetOnBodySetupShapePicked([this](FBodySetupShapeSelection Selection)
	{
		SetSelectedShape(Selection);
	});
	ViewportClient.SetOnBodySetupShapeEdited([this]()
	{
		OnBodySetupShapeEdited();
	});

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FStaticMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FStaticMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FStaticMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FStaticMeshEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 300.0f;
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Static Mesh Editor";
	const FString AssetPath = StaticMesh ? StaticMesh->GetAssetPathFileName() : FString();
	if (!AssetPath.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderTabBar();
	ImGui::Separator();
	if (ActiveTab == EStaticMeshEditorTab::AggregateGeometry)
	{
		RenderAggregateGeometryTab(StaticMesh);
	}
	else
	{
		RenderViewTab(StaticMesh, DetailsWidth);
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FStaticMeshEditorWidget::RenderTabBar()
{
	if (ImGui::BeginTabBar("StaticMeshEditorTabs"))
	{
		if (ImGui::BeginTabItem("View"))
		{
			ActiveTab = EStaticMeshEditorTab::View;
			ViewportClient.SetBodySetupEditingEnabled(false);
			ViewportClient.GetRenderOptions().ShowFlags.bPhysicsBody = false;
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Aggregate Geometry"))
		{
			ActiveTab = EStaticMeshEditorTab::AggregateGeometry;
			ViewportClient.SetBodySetupEditingEnabled(true);
			ViewportClient.GetRenderOptions().ShowFlags.bPhysicsBody = true;
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void FStaticMeshEditorWidget::RenderViewTab(UStaticMesh* StaticMesh, float DetailsWidth)
{
	ImGui::BeginGroup();
	const float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
	RenderViewportPanel(ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y), false);
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Static Mesh Details");
	ImGui::Separator();
	RenderDetailsPanel(StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr);
	ImGui::EndChild();
}

void FStaticMeshEditorWidget::RenderAggregateGeometryTab(UStaticMesh* StaticMesh)
{
	constexpr float ShapeListWidth = 220.0f;
	constexpr float ShapeDetailsWidth = 340.0f;

	ImGui::BeginChild("AggregateGeometryShapes", ImVec2(ShapeListWidth, 0), true);
	RenderAggregateShapeList(StaticMesh);
	ImGui::EndChild();

	ImGui::SameLine();

	const float ViewportWidth = FMath::Max(
		160.0f,
		ImGui::GetContentRegionAvail().x - ShapeDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginGroup();
	RenderViewportPanel(ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y), true);
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("AggregateGeometryShapeDetails", ImVec2(ShapeDetailsWidth, 0), true);
	RenderAggregateShapeDetails(StaticMesh);
	ImGui::EndChild();
}

void FStaticMeshEditorWidget::RenderViewportPanel(ImVec2 Size, bool bShowGizmoControls)
{
	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || Size.x <= 0.0f || Size.y <= 0.0f)
	{
		ImGui::Dummy(Size);
		return;
	}

	VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
	}
	else
	{
		ImGui::Dummy(Size);
	}

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewportPos,
		ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
		IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Gizmo = bShowGizmoControls ? ViewportClient.GetGizmo() : nullptr;
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.bShowGizmoControls = bShowGizmoControls;
	Context.OnCoordSystemToggled = [&]()
	{
		FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
		Settings.CoordSystem = Settings.CoordSystem == EEditorCoordSystem::World
			? EEditorCoordSystem::Local
			: EEditorCoordSystem::World;
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnSettingsChanged = [&]()
	{
		ViewportClient.ApplyTransformSettingsToGizmo();
	};

	FViewportToolbar::Render(Context);
	RenderMeshStatsOverlay(DrawList, ViewportPos);
}

void FStaticMeshEditorWidget::RenderAggregateShapeList(UStaticMesh* StaticMesh)
{
	ImGui::TextUnformatted("Aggregate Geometry");
	ImGui::Separator();

	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup)
	{
		ImGui::TextDisabled("No BodySetup.");
	}
	else
	{
		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.SphereElems.size()); ++Index)
		{
			RenderShapeSelectable("Sphere", { EAggCollisionShape::Sphere, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.BoxElems.size()); ++Index)
		{
			RenderShapeSelectable("Box", { EAggCollisionShape::Box, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.SphylElems.size()); ++Index)
		{
			RenderShapeSelectable("Capsule", { EAggCollisionShape::Sphyl, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.ConvexElems.size()); ++Index)
		{
			RenderShapeSelectable("Convex", { EAggCollisionShape::Convex, Index });
		}
	}

	if (ImGui::BeginPopupContextWindow("##StaticMeshAggregateGeometryContext", ImGuiPopupFlags_MouseButtonRight))
	{
		RenderAddShapeContextMenu(StaticMesh);
		ImGui::EndPopup();
	}
}

void FStaticMeshEditorWidget::RenderAggregateShapeDetails(UStaticMesh* StaticMesh)
{
	FKShapeElem* Shape = GetSelectedShape(StaticMesh);
	if (!Shape)
	{
		ImGui::TextDisabled("Select a shape.");
		return;
	}

	ImGui::TextUnformatted("Shape Details");
	ImGui::SameLine();
	if (ImGui::Button("Delete Selected"))
	{
		DeleteSelectedAggregateShape(StaticMesh);
		return;
	}
	ImGui::Separator();

	UStruct* StructType = nullptr;
	switch (SelectedShape.Type)
	{
	case EAggCollisionShape::Sphere:
		StructType = FKSphereElem::StaticStruct();
		break;
	case EAggCollisionShape::Box:
		StructType = FKBoxElem::StaticStruct();
		break;
	case EAggCollisionShape::Sphyl:
		StructType = FKSphylElem::StaticStruct();
		break;
	case EAggCollisionShape::Convex:
		StructType = FKConvexElem::StaticStruct();
		break;
	default:
		break;
	}

	if (StructType && FInlinePropertyRenderer::RenderStructProperties(StructType, Shape, StaticMesh, "##StaticMeshAggregateShapeProps"))
	{
		SaveStaticMeshChange("StaticMesh AggregateGeom edit warning");
		MarkDirty();
		ViewportClient.MarkBodySetupDebugDirty();
	}
}

bool FStaticMeshEditorWidget::RenderAddShapeContextMenu(UStaticMesh* StaticMesh)
{
	bool bAdded = false;
	if (ImGui::MenuItem("Add Sphere"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Sphere);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Box"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Box);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Capsule"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Sphyl);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Convex"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Convex);
		bAdded = true;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Delete Selected", nullptr, false, GetSelectedShape(StaticMesh) != nullptr))
	{
		bAdded = DeleteSelectedAggregateShape(StaticMesh) || bAdded;
	}
	return bAdded;
}

bool FStaticMeshEditorWidget::RenderShapeSelectable(const char* TypeLabel, FBodySetupShapeSelection Selection)
{
	FString Label = TypeLabel;
	Label += " [";
	Label += std::to_string(Selection.Index);
	Label += "]##StaticMeshAggShape";
	Label += std::to_string(static_cast<int32>(Selection.Type));
	Label += "_";
	Label += std::to_string(Selection.Index);

	const bool bSelected = SelectedShape == Selection;
	if (ImGui::Selectable(Label.c_str(), bSelected))
	{
		SetSelectedShape(Selection);
	}
	return bSelected;
}

void FStaticMeshEditorWidget::AddAggregateShape(UStaticMesh* StaticMesh, EAggCollisionShape Type)
{
	if (!StaticMesh)
	{
		return;
	}

	UBodySetup* BodySetup = StaticMesh->CreateBodySetupIfMissing();
	if (!BodySetup)
	{
		return;
	}

	FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	FBodySetupShapeSelection NewSelection { Type, 0 };
	switch (Type)
	{
	case EAggCollisionShape::Sphere:
		AggGeom.SphereElems.push_back(FKSphereElem());
		NewSelection.Index = static_cast<int32>(AggGeom.SphereElems.size()) - 1;
		break;
	case EAggCollisionShape::Box:
		AggGeom.BoxElems.push_back(FKBoxElem());
		NewSelection.Index = static_cast<int32>(AggGeom.BoxElems.size()) - 1;
		break;
	case EAggCollisionShape::Sphyl:
		AggGeom.SphylElems.push_back(FKSphylElem());
		NewSelection.Index = static_cast<int32>(AggGeom.SphylElems.size()) - 1;
		break;
	case EAggCollisionShape::Convex:
	{
		FKConvexElem ConvexElem;
		FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
		if (!BuildConvexHullFromStaticMesh(MeshAsset, ConvexElem))
		{
			ConvexElem = MakeFallbackConvexElem(MeshAsset);
		}
		AggGeom.ConvexElems.push_back(ConvexElem);
		NewSelection.Index = static_cast<int32>(AggGeom.ConvexElems.size()) - 1;
		break;
	}
	default:
		return;
	}

	SetSelectedShape(NewSelection);
	SaveStaticMeshChange("StaticMesh AggregateGeom add warning");
	MarkDirty();
	ViewportClient.MarkBodySetupDebugDirty();
}

bool FStaticMeshEditorWidget::DeleteSelectedAggregateShape(UStaticMesh* StaticMesh)
{
	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup || !SelectedShape.IsValid())
	{
		return false;
	}

	FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	bool bDeleted = false;
	switch (SelectedShape.Type)
	{
	case EAggCollisionShape::Sphere:
		if (SelectedShape.Index >= 0 && SelectedShape.Index < static_cast<int32>(AggGeom.SphereElems.size()))
		{
			AggGeom.SphereElems.erase(AggGeom.SphereElems.begin() + SelectedShape.Index);
			bDeleted = true;
		}
		break;
	case EAggCollisionShape::Box:
		if (SelectedShape.Index >= 0 && SelectedShape.Index < static_cast<int32>(AggGeom.BoxElems.size()))
		{
			AggGeom.BoxElems.erase(AggGeom.BoxElems.begin() + SelectedShape.Index);
			bDeleted = true;
		}
		break;
	case EAggCollisionShape::Sphyl:
		if (SelectedShape.Index >= 0 && SelectedShape.Index < static_cast<int32>(AggGeom.SphylElems.size()))
		{
			AggGeom.SphylElems.erase(AggGeom.SphylElems.begin() + SelectedShape.Index);
			bDeleted = true;
		}
		break;
	case EAggCollisionShape::Convex:
		if (SelectedShape.Index >= 0 && SelectedShape.Index < static_cast<int32>(AggGeom.ConvexElems.size()))
		{
			AggGeom.ConvexElems.erase(AggGeom.ConvexElems.begin() + SelectedShape.Index);
			bDeleted = true;
		}
		break;
	default:
		break;
	}

	if (!bDeleted)
	{
		return false;
	}

	SetSelectedShape({});
	SaveStaticMeshChange("StaticMesh AggregateGeom delete warning");
	MarkDirty();
	ViewportClient.MarkBodySetupDebugDirty();
	return true;
}

FKShapeElem* FStaticMeshEditorWidget::GetSelectedShape(UStaticMesh* StaticMesh) const
{
	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	return BodySetup && SelectedShape.IsValid()
		? BodySetup->GetAggGeom().GetElement(SelectedShape.Type, SelectedShape.Index)
		: nullptr;
}

void FStaticMeshEditorWidget::SetSelectedShape(FBodySetupShapeSelection Selection)
{
	SelectedShape = Selection;
	ViewportClient.SetSelectedBodySetupShape(SelectedShape);
}

void FStaticMeshEditorWidget::SaveStaticMeshChange(const char* LogPrefix)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);
	if (!StaticMesh)
	{
		return;
	}

	const FString StaticMeshPath = StaticMesh->GetAssetPathFileName();
	if (!FMeshManager::SaveStaticMesh(StaticMesh, StaticMeshPath))
	{
		UE_LOG("%s: failed to persist StaticMesh change. StaticMesh=%s", LogPrefix, StaticMeshPath.c_str());
	}
}

void FStaticMeshEditorWidget::OnBodySetupShapeEdited()
{
	SaveStaticMeshChange("StaticMesh AggregateGeom gizmo edit warning");
	MarkDirty();
	ViewportClient.MarkBodySetupDebugDirty();
}

void FStaticMeshEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;

	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject))
	{
		if (const FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
	}

	const FString Text =
		"Triangles: " + FormatStaticMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatStaticMeshStatCount(VertexCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FStaticMeshEditorWidget::RenderDetailsPanel(FStaticMesh* Asset) const
{
	if (!Asset)
	{
		ImGui::TextDisabled("No static mesh data.");
		return;
	}

	ImGui::Text("Vertices: %s", FormatStaticMeshStatCount(Asset->Vertices.size()).c_str());
	ImGui::Text("Indices: %s", FormatStaticMeshStatCount(Asset->Indices.size()).c_str());
	ImGui::Text("Triangles: %s", FormatStaticMeshStatCount(Asset->Indices.size() / 3).c_str());
	ImGui::Text("Sections: %s", FormatStaticMeshStatCount(Asset->Sections.size()).c_str());
}
