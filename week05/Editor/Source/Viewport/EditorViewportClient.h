#pragma once

#include "ViewportTypes.h"
#include "Core/ViewportClient.h"
#include "Gizmo/Gizmo.h"
#include "Picking/Picker.h"
#include "Types/CoreTypes.h"
#include "Services/EditorViewportInputService.h"
#include "Services/EditorViewportAssetInteractionService.h"
#include "Services/EditorViewportRenderService.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Core/ShowFlags.h"

class FEditorUI;
class FFrustum;
class FEditorEngine;
class FEditorViewportRegistry;
class UWorld;
#include "Renderer/Material.h"

/**
 * 에디터 메인 뷰포트 클라이언트다.
 * 입력, 에셋 상호작용, 기즈모, 뷰포트별 씬 패킷 생성, 에디터 프레임 요청 조립을
 * 여러 서비스 객체에 위임하면서 뷰포트 레벨의 조정 역할을 맡는다.
 */
class FEditorViewportClient : public IViewportClient
{
public:
	FEditorViewportClient(
		FEditorEngine& InEditorEngine,
		FEditorUI& InEditorUI,
		FEditorViewportRegistry& InViewportRegistry,
		FWindowsWindow* InMainWindow);

	// 에디터 뷰포트가 활성화될 때 UI와 렌더 리소스를 연결한다.
	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	// 에디터 뷰포트가 비활성화될 때 리소스와 드래그 상태를 정리한다.
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	// 뷰포트 입력/카메라 내비게이션을 프레임마다 갱신한다.
	void Tick(FEngine* Engine, float DeltaTime) override;
	// 윈도우 메시지를 입력 서비스와 피킹/기즈모 로직으로 전달한다.
	void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) override;
	// 현재 기즈모 모드를 반환한다.
	EGizmoMode GetGizmoMode() const { return Gizmo.GetMode(); }
	// 현재 기즈모 모드를 설정한다.
	void SetGizmoMode(EGizmoMode InMode) const { Gizmo.SetMode(InMode); }
	// 현재 좌표계 모드를 반환한다.
	EGizmoCoordinateSpace GetSpaceMode() const { return Gizmo.GetCoordinateSpace(); }
	// 현재 좌표계 모드를 설정한다.
	void SetSpaceMode(EGizmoCoordinateSpace InSpace) const { Gizmo.SetCoordinateSpace(InSpace); }
	// 현재 렌더 모드를 반환한다.
	ERenderMode GetRenderMode() const { return RenderMode; }
	// 현재 렌더 모드를 변경한다.
	void SetRenderMode(ERenderMode InRenderMode) { RenderMode = InRenderMode; }

	// 파일 더블클릭을 에셋 상호작용 서비스로 전달한다.
	void HandleFileDoubleClick(const FString& FilePath) override;
	// 뷰포트 드롭을 에셋 상호작용 서비스로 전달한다.
	void HandleFileDropOnViewport(const FString& FilePath) override;
	// 뷰포트별 월드 데이터를 씬 패킷으로 수집한다.
	void BuildSceneRenderPacket(
		FEngine* Engine,
		UWorld* World,
		const FFrustum& Frustum,
		const FShowFlags& Flags,
		FSceneRenderPacket& OutPacket) override;
	// 전체 에디터 뷰포트 프레임 요청을 구성해 렌더 서비스에 위임한다.
	void Render(FEngine* Engine, FRenderer* Renderer);

private:
	FEditorUI& EditorUI;
	mutable FGizmo Gizmo;
	// Slate 레이아웃 결과를 실제 뷰포트 사각형에 반영한다.
	void SyncViewportRectsFromDock();

	FWindowsWindow* MainWindow = nullptr;

	FPicker Picker;
	FEditorEngine& EditorEngine;
	FEditorViewportRegistry& ViewportRegistry;
	FEditorViewportInputService InputService;
	FEditorViewportAssetInteractionService AssetInteractionService;
	FEditorViewportRenderService RenderService;

	ERenderMode RenderMode = ERenderMode::Lighting;
	const FString WireframeMaterialName = "M_Wireframe";
	std::shared_ptr<FMaterial> WireFrameMaterial = nullptr;
	std::unique_ptr<FDynamicMesh> GridMesh;
	std::shared_ptr<FMaterial> GridMaterial;
	std::unique_ptr<FDynamicMaterial> GridMaterials[MAX_VIEWPORTS];
	// 에디터 그리드 렌더링에 필요한 메시와 머티리얼을 생성한다.
	void CreateGridResource(FRenderer* Renderer);
};
