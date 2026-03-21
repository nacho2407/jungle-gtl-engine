#include "Editor.h"

void FEditor::Create()
{
    //  TODO : Viewport Client
    
    //  TODO : Panel UI
    
    //  TODO : Gizmo
}

void FEditor::Release()
{
    //  TODO : Call Release Functions
}

void FEditor::Initialize()
{
    InputRouter.AddContext(&EditorGlobalContext);
    InputRouter.AddContext(&ViewPortInputContext);
    InputRouter.AddContext(&GizmoInputContext);
    //  TODO : Scene의 Begin Play 호출
}


void FEditor::Tick(Engine::ApplicationCore::FInputSystem * InputSystem)
{
    //  TODO : Build 오류로 인해 주석 처리 해놓음 이후에 풀기
    // FInputEvent Event;
    //  InputSystem에서 Raw Event 및 State 받기
    // while (InputSystem->PollEvent(&Event))
    // {
    //     InputRouter.RouteEvent(Event, InputSystem->GetInputState());
    // }
    // InputRouter.TickContexts(InputSystem->GetInputState());
    
    //  TODO : Editor Updates
}

void FEditor::OnWindowResized(float Width, float Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }
    
    WindowHeight = Height;
    WindowWidth = Width;
    //  TODO : Setting Panel Size
}

void FEditor::CreateNewScene()
{
    ClearScene();
    //  TODO : 새로운 Scene으로 교체, Panel 초기화, 
}

void FEditor::ClearScene()
{
    //  TODO : Scene에 대한 모든 정보 제거
}
