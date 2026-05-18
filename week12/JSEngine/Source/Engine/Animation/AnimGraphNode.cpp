#include "AnimGraphNode.h"
#include "Serialization/Archive.h"

FString AnimGraphNodeTypeToString(EAnimGraphNodeType Type)
{
    switch (Type)
    {
    case EAnimGraphNodeType::OutputPose: return "OutputPose";
    case EAnimGraphNodeType::SequencePlayer: return "SequencePlayer";
    case EAnimGraphNodeType::StateMachine: return "StateMachine";
    default: return "Unknown";
    }
}

EAnimGraphNodeType AnimGraphNodeTypeFromString(const FString& Value)
{
    if (Value == "OutputPose") return EAnimGraphNodeType::OutputPose;
    if (Value == "StateMachine") return EAnimGraphNodeType::StateMachine;
    return EAnimGraphNodeType::SequencePlayer;
}

FString TransitionConditionTypeToString(EAnimTransitionConditionType Type)
{
    switch (Type)
    {
    case EAnimTransitionConditionType::Always: return "Always";
    case EAnimTransitionConditionType::BoolParameter: return "BoolParameter";
    case EAnimTransitionConditionType::FloatGreater: return "FloatGreater";
    case EAnimTransitionConditionType::FloatLess: return "FloatLess";
    case EAnimTransitionConditionType::LuaFunction: return "LuaFunction";
    default: return "Always";
    }
}

EAnimTransitionConditionType TransitionConditionTypeFromString(const FString& Value)
{
    if (Value == "BoolParameter") return EAnimTransitionConditionType::BoolParameter;
    if (Value == "FloatGreater") return EAnimTransitionConditionType::FloatGreater;
    if (Value == "FloatLess") return EAnimTransitionConditionType::FloatLess;
    if (Value == "LuaFunction") return EAnimTransitionConditionType::LuaFunction;
    return EAnimTransitionConditionType::Always;
}

FArchive& operator<<(FArchive& Ar, FAnimTransitionConditionDesc& Desc)
{
    Ar.BeginObject(Ar.GetCurrentKey());

    FString TypeStr;
    if (Ar.IsSaving())
    {
        TypeStr = TransitionConditionTypeToString(Desc.Type);
    }

    Ar << "Type" << TypeStr;

    if (Ar.IsLoading())
    {
        Desc.Type = TransitionConditionTypeFromString(TypeStr);
    }

    Ar << "ParameterName" << Desc.ParameterName;
    Ar << "BoolValue" << Desc.BoolValue;
    Ar << "Threshold" << Desc.Threshold;
    Ar << "LuaFunctionName" << Desc.LuaFunctionName;

    Ar.EndObject();
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimStateTransitionDesc& Desc)
{
    Ar.BeginObject(Ar.GetCurrentKey());

    Ar << "FromStateId" << Desc.FromStateId;
    Ar << "ToStateId" << Desc.ToStateId;
    Ar << "BlendTime" << Desc.BlendTime;
    Ar << "Priority" << Desc.Priority;
    Ar << "Condition" << Desc.Condition;

    Ar.EndObject();
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimStateDesc& Desc)
{
    Ar.BeginObject(Ar.GetCurrentKey());

    Ar << "StateId" << Desc.StateId;
    Ar << "Name" << Desc.Name;
    Ar << "AnimationPath" << Desc.AnimationPath;
    Ar << "Position" << Desc.Position;

    Ar.EndObject();
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimStateMachineDesc& Desc)
{
    Ar.BeginObject(Ar.GetCurrentKey());

    Ar << "EntryStateId" << Desc.EntryStateId;
    Ar << "States" << Desc.States;
    Ar << "Transitions" << Desc.Transitions;

    Ar.EndObject();
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimGraphNodeDesc& Desc)
{
    Ar.BeginObject(Ar.GetCurrentKey());

    Ar << "NodeId" << Desc.NodeId;

    FString TypeStr;
    if (Ar.IsSaving())
    {
        TypeStr = AnimGraphNodeTypeToString(Desc.Type);
    }

    Ar << "Type" << TypeStr;

    if (Ar.IsLoading())
    {
        Desc.Type = AnimGraphNodeTypeFromString(TypeStr);
    }

    Ar << "Name" << Desc.Name;
    Ar << "Position" << Desc.Position;

    Ar << "AnimationPath" << Desc.AnimationPath;
    Ar << "PlayRate" << Desc.PlayRate;
    Ar << "Loop" << Desc.bLoop;
    Ar << "InputPoseNodeId" << Desc.InputPoseNodeId;

    if (Ar.IsSaving() || Ar.HasKey("StateMachine"))
    {
        Ar << "StateMachine" << Desc.StateMachine;
    }

    Ar.EndObject();
    return Ar;
}