#include "EditorViewportContextMenuTool.h"

#include "Editor/Input/EditorViewportInputController.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

namespace
{
constexpr int32 ContextClickDragThreshold = 4;
constexpr int32 ContextClickDragThresholdSq = ContextClickDragThreshold * ContextClickDragThreshold; // sqrt 사용을 피하기 위해 제곱값을 미리 계산
}

bool FEditorViewportContextMenuTool::HandleInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!Owner || !Controller)
    {
        bRightClickCandidate = false;
        return false;
    }

    const FEditorViewportFrameInput& Input = Controller->GetCurrentInput();

	// 오른쪽 마우스 버튼이 클릭되면 일단 컨텍스트 메뉴 후보 입력으로 표시
    if (Input.bRightPressed)
    {
        bRightClickCandidate = true;
        RightClickStartLocalPos = Input.MouseLocalPos;
        return false;
    }

	// 다음 프레임에서 오른쪽 마우스 버튼이 여전히 클릭된 상태라면, 마우스가 일정 거리 이상 이동했는지 확인하여 컨텍스트 메뉴 후보 입력을 취소
    if (bRightClickCandidate && Input.bRightDown)
    {
        const int32 DeltaX = Input.MouseLocalPos.x - RightClickStartLocalPos.x;
        const int32 DeltaY = Input.MouseLocalPos.y - RightClickStartLocalPos.y;
        if (DeltaX * DeltaX + DeltaY * DeltaY > ContextClickDragThresholdSq)
        {
            bRightClickCandidate = false;
        }

        return false;
    }

	// 오른쪽 마우스 버튼이 해제되면, 컨텍스트 메뉴 후보 입력이 여전히 유효한지 확인하여 컨텍스트 메뉴 요청 처리
    if (Input.bRightReleased)
    {
        const bool bShouldOpenContextMenu = bRightClickCandidate;
        bRightClickCandidate = false;
        if (bShouldOpenContextMenu)
        {
            return HandleContextMenuRequest();
        }
    }

    return false;
}

bool FEditorViewportContextMenuTool::HandleContextMenuRequest()
{
    const FEditorViewportFrameInput& Input = Controller->GetCurrentInput();
    const bool bCanStopPiloting = Owner->IsPilotingActor();

    AActor* HitActor = nullptr;
    UWorld* World = Owner->GetWorld();
    if (World && World->GetWorldType() != EWorldType::PIE)
    {
        FRay Ray;
        if (BuildMouseRay(Ray))
        {
            FHitResult HitResult{};
            World->RaycastEditorPicking(Ray, HitResult, HitActor);
        }
    }

    if (!HitActor && !bCanStopPiloting)
    {
        return false;
    }

    FEditorViewportContextMenuRequest Request{};
    Request.HitActor = HitActor;
    Request.ScreenPos = Input.MouseScreenPos;
    Request.ClientPos = Input.MouseClientPos;
    Request.LocalPos = Input.MouseLocalPos;
    Request.bCanStopPiloting = bCanStopPiloting;
    Controller->RequestContextMenu(Request);
    return true;
}
