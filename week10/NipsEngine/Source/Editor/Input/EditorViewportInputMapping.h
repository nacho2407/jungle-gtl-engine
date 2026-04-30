#pragma once

#include "Engine/Input/InputBinding.h"

namespace EditorViewportInputMapping
{
    enum class EEditorViewportAction : int32
    {
        CycleMode,
        SetModeSelect,
        SetModeTranslate,
        SetModeRotate,
        SetModeScale,
        CycleGizmoMode,
        ToggleCoordinateSpace,
        FocusSelection,
        DeleteSelection,
        SelectAll,
        NewScene,
        LoadScene,
        SaveScene,
        SaveSceneAs,
        TogglePIEPossessEject,
        DuplicateSelection,
        EndPIE,
        SelectPrimaryReleased,
        SelectToggleReleased,
        SelectAddReleased,
        BoxSelectReplaceDown,
        BoxSelectAdditiveDown,
        NavOrbitAltLeftDown,
        NavDollyAltRightDown,
        NavPanAltMiddleDown,
        NavLookRightDown,
        NavLookMiddleDown,
        NavMoveForward,
        NavMoveLeft,
        NavMoveBackward,
        NavMoveRight,
        NavMoveDown,
        NavMoveUp,
        NavRotateUp,
        NavRotateLeft,
        NavRotateDown,
        NavRotateRight,
        NavWheelScroll
    };

    inline const TArray<FInputBinding>& GetBindings()
    {
        static const TArray<FInputBinding> Bindings =
        {
            { static_cast<int32>(EEditorViewportAction::CycleMode), EInputBindingTrigger::Released, { VK_TAB, false, false, false }, EInputEventType::KeyReleased, 120 },
            { static_cast<int32>(EEditorViewportAction::SetModeSelect), EInputBindingTrigger::Released, { '1', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeSelect), EInputBindingTrigger::Released, { 'Q', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeTranslate), EInputBindingTrigger::Released, { '2', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeTranslate), EInputBindingTrigger::Released, { 'W', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeRotate), EInputBindingTrigger::Released, { '3', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeRotate), EInputBindingTrigger::Released, { 'E', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeScale), EInputBindingTrigger::Released, { '4', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::SetModeScale), EInputBindingTrigger::Released, { 'R', false, false, false }, EInputEventType::KeyReleased, 121 },
            { static_cast<int32>(EEditorViewportAction::CycleGizmoMode), EInputBindingTrigger::Released, { VK_SPACE, false, false, false }, EInputEventType::KeyReleased, 110 },
            { static_cast<int32>(EEditorViewportAction::ToggleCoordinateSpace), EInputBindingTrigger::Released, { 'X', false, false, false }, EInputEventType::KeyReleased, 111 },
            { static_cast<int32>(EEditorViewportAction::FocusSelection), EInputBindingTrigger::Released, { 'F', false, false, false }, EInputEventType::KeyReleased, 100 },
            { static_cast<int32>(EEditorViewportAction::DeleteSelection), EInputBindingTrigger::Released, { VK_DELETE, false, false, false }, EInputEventType::KeyReleased, 100 },
            { static_cast<int32>(EEditorViewportAction::SelectAll), EInputBindingTrigger::Pressed, { 'A', true, false, false }, EInputEventType::KeyPressed, 105 },
            { static_cast<int32>(EEditorViewportAction::NewScene), EInputBindingTrigger::Pressed, { 'N', true, false, false }, EInputEventType::KeyPressed, 105 },
            { static_cast<int32>(EEditorViewportAction::LoadScene), EInputBindingTrigger::Pressed, { 'O', true, false, false }, EInputEventType::KeyPressed, 105 },
            { static_cast<int32>(EEditorViewportAction::SaveScene), EInputBindingTrigger::Pressed, { 'S', true, false, false }, EInputEventType::KeyPressed, 105 },
            { static_cast<int32>(EEditorViewportAction::SaveSceneAs), EInputBindingTrigger::Pressed, { 'S', true, true, false }, EInputEventType::KeyPressed, 106 },
            { static_cast<int32>(EEditorViewportAction::TogglePIEPossessEject), EInputBindingTrigger::Pressed, { VK_F8, false, false, false }, EInputEventType::KeyPressed, 190 },
            { static_cast<int32>(EEditorViewportAction::DuplicateSelection), EInputBindingTrigger::Pressed, { 'D', true, false, false }, EInputEventType::KeyPressed, 130 },
            { static_cast<int32>(EEditorViewportAction::EndPIE), EInputBindingTrigger::Pressed, { VK_ESCAPE, false, false, false }, EInputEventType::KeyPressed, 200 },
            { static_cast<int32>(EEditorViewportAction::SelectPrimaryReleased), EInputBindingTrigger::Released, { VK_LBUTTON, false, false, false }, EInputEventType::KeyReleased, 10 },
            { static_cast<int32>(EEditorViewportAction::SelectToggleReleased), EInputBindingTrigger::Released, { VK_LBUTTON, true, false, false }, EInputEventType::KeyReleased, 20 },
            { static_cast<int32>(EEditorViewportAction::SelectAddReleased), EInputBindingTrigger::Released, { VK_LBUTTON, false, false, true }, EInputEventType::KeyReleased, 20 },
            { static_cast<int32>(EEditorViewportAction::BoxSelectReplaceDown), EInputBindingTrigger::Down, { VK_LBUTTON, true, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::BoxSelectAdditiveDown), EInputBindingTrigger::Down, { VK_LBUTTON, true, true, true }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavOrbitAltLeftDown), EInputBindingTrigger::Down, { VK_LBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavDollyAltRightDown), EInputBindingTrigger::Down, { VK_RBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavPanAltMiddleDown), EInputBindingTrigger::Down, { VK_MBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavLookRightDown), EInputBindingTrigger::Down, { VK_RBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavLookMiddleDown), EInputBindingTrigger::Down, { VK_MBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveForward), EInputBindingTrigger::Down, { 'W', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveLeft), EInputBindingTrigger::Down, { 'A', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveBackward), EInputBindingTrigger::Down, { 'S', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveRight), EInputBindingTrigger::Down, { 'D', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveDown), EInputBindingTrigger::Down, { 'Q', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveUp), EInputBindingTrigger::Down, { 'E', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateUp), EInputBindingTrigger::Down, { VK_UP, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateLeft), EInputBindingTrigger::Down, { VK_LEFT, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateDown), EInputBindingTrigger::Down, { VK_DOWN, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateRight), EInputBindingTrigger::Down, { VK_RIGHT, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavWheelScroll), EInputBindingTrigger::EventType, { 0, false, false, false }, EInputEventType::WheelScrolled }
        };
        return Bindings;
    }

    inline int32 GetActionCount()
    {
        return static_cast<int32>(EEditorViewportAction::NavWheelScroll) + 1;
    }

    inline const TArray<const FInputBinding*>& GetBindingsForAction(EEditorViewportAction Action)
    {
        static const TArray<TArray<const FInputBinding*>> BindingsByAction = []()
        {
            TArray<TArray<const FInputBinding*>> Out;
            Out.resize(GetActionCount());
            const TArray<FInputBinding>& All = GetBindings();
            for (const FInputBinding& Binding : All)
            {
                if (Binding.ActionId < 0 || Binding.ActionId >= static_cast<int32>(Out.size()))
                {
                    continue;
                }
                Out[Binding.ActionId].push_back(&Binding);
            }
            return Out;
        }();

        const int32 ActionId = static_cast<int32>(Action);
        if (ActionId < 0 || ActionId >= static_cast<int32>(BindingsByAction.size()))
        {
            static const TArray<const FInputBinding*> Empty;
            return Empty;
        }
        return BindingsByAction[ActionId];
    }

    inline bool IsTriggered(const FViewportInputContext& Context, EEditorViewportAction Action)
    {
        return InputBindingUtils::IsActionTriggered(Context, GetBindingsForAction(Action));
    }
}
