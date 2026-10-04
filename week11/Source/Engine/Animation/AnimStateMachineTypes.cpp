#include "Animation/AnimStateMachineTypes.h"

bool FAnimStateMachineDesc::IsAnyStateName(const FName& StateName)
{
    if (!StateName.IsValid())
    {
        return false;
    }

    static const FName AnyStateName("Any");
    static const FName WildcardStateName("*");
    return StateName == AnyStateName || StateName == WildcardStateName;
}

FAnimStateDesc* FAnimStateMachineDesc::FindStateById(int32 StateId)
{
    for (FAnimStateDesc& State : States)
    {
        if (State.Id == StateId)
        {
            return &State;
        }
    }

    return nullptr;
}

const FAnimStateDesc* FAnimStateMachineDesc::FindStateById(int32 StateId) const
{
    for (const FAnimStateDesc& State : States)
    {
        if (State.Id == StateId)
        {
            return &State;
        }
    }

    return nullptr;
}

FAnimStateDesc* FAnimStateMachineDesc::FindStateByName(const FName& StateName)
{
    if (!StateName.IsValid())
    {
        return nullptr;
    }

    for (FAnimStateDesc& State : States)
    {
        if (State.Name == StateName)
        {
            return &State;
        }
    }

    return nullptr;
}

const FAnimStateDesc* FAnimStateMachineDesc::FindStateByName(const FName& StateName) const
{
    if (!StateName.IsValid())
    {
        return nullptr;
    }

    for (const FAnimStateDesc& State : States)
    {
        if (State.Name == StateName)
        {
            return &State;
        }
    }

    return nullptr;
}

bool FAnimStateMachineDesc::HasState(const FName& StateName) const
{
    return FindStateByName(StateName) != nullptr;
}

FAnimTransitionDesc* FAnimStateMachineDesc::FindTransitionById(int32 TransitionId)
{
    for (FAnimTransitionDesc& Transition : Transitions)
    {
        if (Transition.Id == TransitionId)
        {
            return &Transition;
        }
    }

    return nullptr;
}

const FAnimTransitionDesc* FAnimStateMachineDesc::FindTransitionById(int32 TransitionId) const
{
    for (const FAnimTransitionDesc& Transition : Transitions)
    {
        if (Transition.Id == TransitionId)
        {
            return &Transition;
        }
    }

    return nullptr;
}

void FAnimStateMachineDesc::CollectTransitionsFromState(
    const FName& FromState,
    TArray<const FAnimTransitionDesc*>& OutTransitions) const
{
    OutTransitions.clear();
    if (!FromState.IsValid())
    {
        return;
    }

    for (const FAnimTransitionDesc& Transition : Transitions)
    {
        if (Transition.FromState == FromState)
        {
            OutTransitions.push_back(&Transition);
        }
    }
}

void FAnimStateMachineDesc::CollectAnyTransitions(TArray<const FAnimTransitionDesc*>& OutTransitions) const
{
    OutTransitions.clear();
    for (const FAnimTransitionDesc& Transition : Transitions)
    {
        if (IsAnyStateName(Transition.FromState))
        {
            OutTransitions.push_back(&Transition);
        }
    }
}
