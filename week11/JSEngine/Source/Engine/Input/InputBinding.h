#pragma once

#include <limits>

#include "Engine/Input/InputTypes.h"

enum class EInputBindingTrigger : uint8
{
    Pressed,
    Down,
    Released,
    EventType
};

struct FInputBinding
{
    int32                ActionId = 0;
    EInputBindingTrigger Trigger = EInputBindingTrigger::Pressed;
    FInputChord          Chord{};
    EInputEventType      EventType = EInputEventType::KeyPressed;
    int32                Priority = 0;
};

namespace InputBindingUtils
{
    inline bool IsBindingTriggered(const FViewportInputContext& Context, const FInputBinding& Binding)
    {
        switch (Binding.Trigger)
        {
        case EInputBindingTrigger::Pressed:
            for (const FInputEvent& Event : Context.Events)
            {
                if (Event.Type == EInputEventType::KeyPressed && Event.Key == Binding.Chord.Key && Binding.Chord.MatchesState(Context.Frame))
                {
                    return true;
                }
            }
            return false;
        case EInputBindingTrigger::Down:
            return Context.Frame.IsDown(Binding.Chord.Key) && Binding.Chord.MatchesState(Context.Frame);
        case EInputBindingTrigger::Released:
            for (const FInputEvent& Event : Context.Events)
            {
                if (Event.Type == EInputEventType::KeyReleased && Event.Key == Binding.Chord.Key && Binding.Chord.MatchesState(Context.Frame))
                {
                    return true;
                }
            }
            return false;
        case EInputBindingTrigger::EventType:
            for (const FInputEvent& Event : Context.Events)
            {
                if (Event.Type != Binding.EventType)
                {
                    continue;
                }
                if (Binding.Chord.Key != 0 && Event.Key != Binding.Chord.Key)
                {
                    continue;
                }
                if (!Binding.Chord.MatchesState(Context.Frame))
                {
                    continue;
                }
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    inline bool IsActionTriggered(const FViewportInputContext& Context, const TArray<FInputBinding>& Bindings, int32 ActionId)
    {
        for (const FInputBinding& Binding : Bindings)
        {
            if (Binding.ActionId == ActionId && IsBindingTriggered(Context, Binding))
            {
                return true;
            }
        }
        return false;
    }

    inline bool IsActionTriggered(const FViewportInputContext& Context, const TArray<const FInputBinding*>& Bindings)
    {
        for (const FInputBinding* Binding : Bindings)
        {
            if (Binding && IsBindingTriggered(Context, *Binding))
            {
                return true;
            }
        }
        return false;
    }

    inline bool TryGetHighestPriorityTriggeredAction(
        const FViewportInputContext& Context,
        const TArray<FInputBinding>& Bindings,
        const TArray<int32>& CandidateActionIds,
        int32& OutActionId)
    {
        bool  bFound = false;
        int32 BestPriority = (std::numeric_limits<int32>::min)();
        int32 BestActionId = 0;

        for (const int32 CandidateActionId : CandidateActionIds)
        {
            for (const FInputBinding& Binding : Bindings)
            {
                if (Binding.ActionId != CandidateActionId || !IsBindingTriggered(Context, Binding))
                {
                    continue;
                }
                if (!bFound || Binding.Priority > BestPriority)
                {
                    bFound = true;
                    BestPriority = Binding.Priority;
                    BestActionId = CandidateActionId;
                }
            }
        }

        if (bFound)
        {
            OutActionId = BestActionId;
        }
        return bFound;
    }
}

