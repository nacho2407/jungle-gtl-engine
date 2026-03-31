#include "EditorGlobalContext.h"
#include "Input/EditorGlobalContext.h"

#include "Editor/Editor.h"
#include "Engine/Game/Actor.h"
#include "Viewport/Global/EditorGlobalController.h"
#include "Viewport/Navigation/ViewportNavigationController.h"
#include "Viewport/EditorViewportClient.h"
#include "Panel/PanelManager.h"
#include "Panel/PropertiesPanel.h"
#include "Panel/OutlinerPanel.h"

#include "imgui.h"

using Engine::ApplicationCore::EInputEventType;
using Engine::ApplicationCore::EKey;

bool FEditorGlobalContext::HandleEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                       const Engine::ApplicationCore::FInputState& State)
{
    if (Event.Type != EInputEventType::KeyDown || Event.bRepeat)
    {
        return false;
    }

    if (ImGui::GetCurrentContext() != nullptr)
    {
        const ImGuiIO& IO = ImGui::GetIO();
        if (IO.WantCaptureKeyboard || IO.WantTextInput)
        {
            return false;
        }
    }

    if (Controller == nullptr)
    {
        return false;
    }

    const bool bCtrlDown = State.Modifiers.bCtrlDown;
    const bool bShiftDown = State.Modifiers.bShiftDown;
    const bool bAltDown = State.Modifiers.bAltDown;

    // --- CTRL Shortcuts ---
    if (bCtrlDown && !bAltDown)
    {
        if (Event.Key == EKey::S)
        {
            if (bShiftDown)
            {
                Controller->SaveSceneAs();
            }
            else
            {
                Controller->SaveScene();
            }

            return true;
        }

        if (!bShiftDown && Event.Key == EKey::O)
        {
            Controller->OpenScene();
            return true;
        }

        if (!bShiftDown && Event.Key == EKey::N)
        {
            Controller->NewScene();
            return true;
        }
    }

    // --- Standard Shortcuts (No Modifiers) ---
    if (!bCtrlDown && !bShiftDown && !bAltDown)
    {
        // Help / About
        if (Event.Key == EKey::F1)
        {
            Controller->RequestAboutPopup();
            return true;
        }

        // Camera Focus
        if (Event.Key == EKey::F)
        {
            if (NavigationController != nullptr)
            {
                NavigationController->FocusActors();
                return true;
            }
        }

        // Rename (F2)
        if (Event.Key == EKey::F2)
        {
            if (Controller->GetEditorContext() && Controller->GetEditorContext()->Editor)
            {
                FEditor* Editor = Controller->GetEditorContext()->Editor;
                if (auto* PanelManager = Editor->GetPanelManager())
                {
                    // PropertiesPanel에서 컴포넌트 이름 변경 시작 시도
                    FPanelOpenRequest PropReq;
                    PropReq.PanelType = std::type_index(typeid(FPropertiesPanel));
                    if (auto* PropPanel =
                            static_cast<FPropertiesPanel*>(PanelManager->FindPanel(PropReq)))
                    {
                        if (auto* SelectedComp = Cast<Engine::Component::USceneComponent>(
                                Editor->GetSelectedObject()))
                        {
                            PropPanel->StartRenaming(SelectedComp);
                            return true;
                        }
                    }

                    // OutlinerPanel에서 액터 이름 변경 시작 시도
                    FPanelOpenRequest OutReq;
                    OutReq.PanelType = std::type_index(typeid(FOutlinerPanel));
                    if (auto* OutPanel =
                            static_cast<FOutlinerPanel*>(PanelManager->FindPanel(OutReq)))
                    {
                        if (auto* SelectedActor = Cast<AActor>(Editor->GetSelectedObject()))
                        {
                            OutPanel->StartRenaming(SelectedActor);
                            return true;
                        }
                    }
                }
            }
        }

        // Gizmo Modes (W, E, R)
        // 카메라 이동 조작(마우스 우클릭 중)일 때는 기즈모 전환을 무시하여 충돌을 방지합니다.
        if (!State.IsKeyDown(EKey::MouseRight) &&
            (Event.Key == EKey::W || Event.Key == EKey::E || Event.Key == EKey::R))
        {
            if (Controller->GetEditorContext() && Controller->GetEditorContext()->Editor)
            {
                FEditor* Editor = Controller->GetEditorContext()->Editor;

                auto& Gizmo = Editor->GetViewportClient().GetGizmoController();

                if (Event.Key == EKey::W)
                    Gizmo.SetGizmoType(EGizmoType::Translation);

                else if (Event.Key == EKey::E)
                    Gizmo.SetGizmoType(EGizmoType::Rotation);

                else if (Event.Key == EKey::R)
                    Gizmo.SetGizmoType(EGizmoType::Scaling);
            }
        }

        // View Modes (1, 2, 3)
        if (Event.Key == EKey::N1 || Event.Key == EKey::N2 || Event.Key == EKey::N3)
        {
            if (Controller->GetEditorContext() && Controller->GetEditorContext()->Editor)
            {
                FEditor* Editor = Controller->GetEditorContext()->Editor;
                auto&    Setting = Editor->GetViewportClient().GetRenderSetting();
                if (Event.Key == EKey::N1)
                    Setting.SetViewMode(EViewModeIndex::VMI_Lit);
                else if (Event.Key == EKey::N2)
                    Setting.SetViewMode(EViewModeIndex::VMI_Unlit);
                else if (Event.Key == EKey::N3)
                    Setting.SetViewMode(EViewModeIndex::VMI_Wireframe);
                return true;
            }
        }

        // Delete Selection
        if (Event.Key == EKey::Delete)
        {
            if (Controller->CanDeleteSelectedActors())
            {
                return Controller->DeleteSelectedActors();
            }
        }
    }

    return false;
}

void FEditorGlobalContext::Tick(const Engine::ApplicationCore::FInputState& State) {}
