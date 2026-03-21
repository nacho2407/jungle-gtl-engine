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
    //  TODO : Scene의 Begin Play 호출
    InputRouter.AddContext(&EditorGlobalContext);
    InputRouter.AddContext(&ViewPortInputContext);
    InputRouter.AddContext(&GizmoInputContext);
}


void FEditor::Tick(Engine::ApplicationCore::FInputSystem * InputSystem)
{
     Engine::ApplicationCore::FInputEvent Event;
    
     while (InputSystem->PollEvent(Event))
     {
         InputRouter.RouteEvent(Event, InputSystem->GetInputState());
     }
     InputRouter.TickContexts(InputSystem->GetInputState());
    
    //  TODO : Editor Updates
}

//void FEditor::OnWindowResized(float Width, float Height)
//{
//    if (Width <= 0 || Height <= 0)
//    {
//        return;
//    }
//    
//    WindowHeight = Height;
//    WindowWidth = Width;
//    //  TODO : Setting Panel Size
//}

void FEditor::CreateNewScene()
{
    ClearScene();
    //  TODO : 새로운 Scene으로 교체, Panel 초기화, 
}

void FEditor::ClearScene()
{
    //  TODO : Scene에 대한 모든 정보 제거
}
