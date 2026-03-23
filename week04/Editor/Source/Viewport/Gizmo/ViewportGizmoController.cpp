#include "ViewportGizmoController.h"
#include "Camera/ViewportCamera.h"

// TODO: 필요한 엔진 헤더 인클루드 (예: Camera, SelectionController, Math Library)

void FViewportGizmoController::Tick(float DeltaTime)
{
    // 현재 기즈모 컨트롤러 자체의 상태 업데이트가 필요하다면 여기서 처리합니다.
    // 대부분의 조작은 마우스 이벤트 기반으로 처리되므로 비워둘 수 있습니다.
}

void FViewportGizmoController::OnMouseButtonDown(const FVector2& MousePos)
{
    if (bIsDragging)
    {
        return;
    }

    // 1. 마우스가 기즈모의 어떤 축을 클릭했는지 판정
    ActiveAxisIndex = HitTestGizmo(MousePos);

    if (ActiveAxisIndex != -1)
    {
        // 2. 드래그 시작 상태 설정
        bIsDragging = true;
        StartMousePos = MousePos;

        // TODO: SelectionController 등을 통해 현재 선택된 오브젝트의 Transform을 가져와서 저장
        // StartTransform = SelectedObject->GetTransform();
    }
}

void FViewportGizmoController::OnMouseButtonUp()
{
    // 드래그 상태 해제 및 활성화된 축 초기화
    bIsDragging = false;
    ActiveAxisIndex = -1;

    // TODO: 드래그가 끝났을 때의 최종 결과를 Undo/Redo 시스템에 기록하는 로직 추가
}

void FViewportGizmoController::OnMouseMove(const FVector2& MousePos)
{
    if (bIsDragging)
    {
        // 드래그 중일 때는 대상의 트랜스폼을 업데이트
        UpdateDrag(MousePos);
    }
    else
    {
        // 드래그 중이 아닐 때는 마우스 Hover(강조) 처리를 위해 Hit Test 수행
        // 이를 통해 렌더러가 특정 축을 노란색 등으로 하이라이트하게 만들 수 있습니다.
        ActiveAxisIndex = HitTestGizmo(MousePos);
    }
}

int32 FViewportGizmoController::HitTestGizmo(const FVector2& MousePos)
{
    if (ViewportCamera == nullptr)
    {
        return -1;
    }

    Geometry::FRay Ray{ViewportCamera->ScreenPointToRay(MousePos)};
   switch (CurrentMode)
    {
    case EGizmoMode::Translate:
        // TODO: Ray vs 3개의 직육면체(또는 원기둥) 교차 판정
        // 카메라에서 가장 가까운(Hit Distance가 가장 짧은) 축의 인덱스 반환

        break;

    case EGizmoMode::Rotate:
        // TODO: Ray vs 3개의 평면/링 교차 판정
        break;

    case EGizmoMode::Scale:
        // TODO: Ray vs 3개의 직육면체 교차 판정 (+ 중앙의 균일 스케일 박스)
        break;
    }
    return -1;
}

void FViewportGizmoController::UpdateDrag(const FVector2& MousePos)
{
    if (ActiveAxisIndex == -1)
    {
        return;
    }

    // 1. 마우스 이동량 계산
    FVector2 MouseDelta = MousePos - StartMousePos; // 필요시 구현된 벡터 뺄셈 연산자 사용

    // TODO: 현재 활성화된 축의 3D 월드 벡터를 화면 2D 공간으로 투영(Project)하여
    // 마우스 이동 방향과 축 방향 간의 내적(Dot Product)을 구해 실제 적용할 스칼라(이동량)를
    // 계산합니다.
    float DeltaValue = 0.0f; // 계산된 조작량

    // 2. 스냅(Snapping) 적용
    if (bEnableSnapping && SnapValue > 0.0f)
    {
        // DeltaValue = FMath::Round(DeltaValue / SnapValue) * SnapValue;
    }

    // 3. 모드에 따른 트랜스폼 계산
    FTransform NewTransform = StartTransform;

    switch (CurrentMode)
    {
    case EGizmoMode::Translate:
        // TODO: 위치 업데이트
        // NewTransform.Location += (AxisVector * DeltaValue);
        break;

    case EGizmoMode::Rotate:
        // TODO: 회전 업데이트
        // NewTransform.Rotation = (StartTransform.Rotation * FQuat(AxisVector, DeltaValue));
        break;

    case EGizmoMode::Scale:
        // TODO: 스케일 업데이트
        // NewTransform.Scale += (AxisVector * DeltaValue);
        break;

    case EGizmoMode::None:
    default:
        break;
    }

    // TODO: 계산된 NewTransform을 선택된 오브젝트에 적용
    // SelectedObject->SetTransform(NewTransform);
}
