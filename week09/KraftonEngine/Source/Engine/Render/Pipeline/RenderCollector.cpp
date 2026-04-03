#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Pipeline/FScene.h"
#include "Render/Pipeline/PrimitiveSceneProxy.h"

#include <algorithm>

void FRenderCollector::CollectWorld(UWorld* World, FRenderBus& RenderBus)
{
	if (!World) return;

	// 프록시 기반 수집: FScene에서 캐싱된 렌더 데이터를 직접 제출
	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();
	CollectFromScene(Scene, RenderBus);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, ELevelViewportType ViewportType, FRenderBus& RenderBus)
{
	if (!Gizmo) return;

	Gizmo->UpdateAxisMask(ViewportType);
	Gizmo->CollectRender(RenderBus);
}

void FRenderCollector::CollectOverlayText(bool bActive, const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus)
{
	if (!bActive) return;

	const TArray<FOverlayStatLine> Lines = OverlaySystem.BuildLines(Editor);
	const float TextScale = OverlaySystem.GetLayout().TextScale;

	for (const FOverlayStatLine& Line : Lines)
	{
		FFontEntry Entry = {};
		Entry.Font.Text = Line.Text;
		Entry.Font.Font = nullptr;
		Entry.Font.Scale = TextScale;
		Entry.Font.bScreenSpace = 1;
		Entry.Font.ScreenPosition = Line.ScreenPosition;

		RenderBus.AddOverlayFontEntry(std::move(Entry));
	}
}

// ============================================================
// 기존 Actor 순회 경로 (per-viewport 프록시 폴백용)
// ============================================================
void FRenderCollector::CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		if (!Primitive->IsVisible()) continue;

		Primitive->CollectRender(RenderBus);

		if (bSelected)
		{
			Primitive->CollectSelection(RenderBus);
		}
	}
}

// ============================================================
// FScene 프록시 기반 수집 — CollectFromActor의 고성능 대체
// ============================================================
void FRenderCollector::CollectFromScene(FScene& Scene, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bPrimitives) return;

	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (!Proxy) continue;
		if (!Proxy->bVisible) continue;
		if (!Proxy->Owner) continue;

		// per-viewport 프록시(Gizmo, Billboard)는 기존 경로로 폴백
		if (Proxy->bPerViewportUpdate)
		{
			Proxy->Owner->CollectRender(RenderBus);

			if (Proxy->bSelected)
			{
				Proxy->Owner->CollectSelection(RenderBus);
			}
			continue;
		}

		// Actor 가시성 체크
		AActor* OwnerActor = Proxy->Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible()) continue;

		// 프록시 캐싱 데이터 → FRenderCommand 제출
		SubmitProxy(Proxy, RenderBus);

		// 선택된 오브젝트 → SelectionMask 제출
		if (Proxy->bSelected)
		{
			Proxy->Owner->CollectSelection(RenderBus);
		}
	}
}

// ============================================================
// SubmitProxy — 프록시 캐싱 데이터를 FRenderCommand로 변환하여 Bus에 제출
// ============================================================
void FRenderCollector::SubmitProxy(const FPrimitiveSceneProxy* Proxy, FRenderBus& RenderBus)
{
	if (!Proxy->MeshBuffer) return;

	FRenderCommand Cmd = {};
	Cmd.Shader = Proxy->Shader;
	Cmd.MeshBuffer = Proxy->MeshBuffer;
	Cmd.PerObjectConstants = Proxy->PerObjectConstants;
	Cmd.SectionDraws = Proxy->SectionDraws;
	Cmd.ExtraCB = Proxy->ExtraCB;

	RenderBus.AddCommand(Proxy->Pass, Cmd);
}
