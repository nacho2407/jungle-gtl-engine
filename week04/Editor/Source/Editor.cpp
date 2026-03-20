#include "Editor.h"

void FEditor::Create(HWND HWindow)
{
    //  TODO : Viewport Client
    
    //  TODO : Panel UI
    
    //  TODO : Gizmo
}

void FEditor::Release()
{
    //  TODO : Call Release Functions
}

void FEditor::BeginPlay()
{
    //  TODO : Scene의 Begin Play 호출
}


void FEditor::Update()
{
    //  Input
    
    //  Update World
    
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
