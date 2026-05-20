#include "Asset/AnimStateMachineLoader.h"

#include "Animation/AnimStateMachine.h"
#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
    constexpr int32 AnimStateMachineAssetVersion = 1;

    FString ToLowerString(FString Text)
    {
        std::transform(
            Text.begin(),
            Text.end(),
            Text.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Text;
    }

    bool IsValidAnimStateName(const FName& StateName)
    {
        return StateName.IsValid() && StateName != FName::None;
    }

    bool IsJsonObject(const json::JSON& Node)
    {
        return Node.JSONType() == json::JSON::Class::Object;
    }

    bool IsJsonArray(const json::JSON& Node)
    {
        return Node.JSONType() == json::JSON::Class::Array;
    }

    bool ReadStringValue(json::JSON& Node, FString& OutValue)
    {
        if (Node.JSONType() != json::JSON::Class::String)
        {
            return false;
        }

        OutValue = Node.ToString();
        return true;
    }

    bool ReadIntValue(json::JSON& Node, int32& OutValue)
    {
        if (Node.JSONType() != json::JSON::Class::Integral)
        {
            return false;
        }

        OutValue = static_cast<int32>(Node.ToInt());
        return true;
    }

    bool ReadFloatValue(json::JSON& Node, float& OutValue)
    {
        // SimpleJSON은 1과 1.0을 서로 다른 타입으로 보관하기 때문에 둘 다 float 입력으로 허용
        if (Node.JSONType() == json::JSON::Class::Floating)
        {
            OutValue = static_cast<float>(Node.ToFloat());
            return true;
        }

        if (Node.JSONType() == json::JSON::Class::Integral)
        {
            OutValue = static_cast<float>(Node.ToInt());
            return true;
        }

        return false;
    }

    bool ReadBoolValue(json::JSON& Node, bool& OutValue)
    {
        if (Node.JSONType() != json::JSON::Class::Boolean)
        {
            return false;
        }

        OutValue = Node.ToBool();
        return true;
    }

    bool ReadRequiredString(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        FString& OutValue)
    {
        if (!Object.hasKey(Key) || !ReadStringValue(Object[Key], OutValue) || OutValue.empty())
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Missing or invalid string. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadRequiredInt(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        int32& OutValue)
    {
        if (!Object.hasKey(Key) || !ReadIntValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Missing or invalid int. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadRequiredFloat(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        float& OutValue)
    {
        if (!Object.hasKey(Key) || !ReadFloatValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Missing or invalid number. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadRequiredBool(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        bool& OutValue)
    {
        if (!Object.hasKey(Key) || !ReadBoolValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Missing or invalid bool. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadOptionalString(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        FString& OutValue)
    {
        if (!Object.hasKey(Key))
        {
            return true;
        }

        if (!ReadStringValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid string. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadOptionalInt(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        int32& OutValue)
    {
        if (!Object.hasKey(Key))
        {
            return true;
        }

        if (!ReadIntValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid int. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadOptionalFloat(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        float& OutValue)
    {
        if (!Object.hasKey(Key))
        {
            return true;
        }

        if (!ReadFloatValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid number. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ReadOptionalBool(
        json::JSON& Object,
        const char* Key,
        const FString& Path,
        const char* Context,
        bool& OutValue)
    {
        if (!Object.hasKey(Key))
        {
            return true;
        }

        if (!ReadBoolValue(Object[Key], OutValue))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid bool. Path=%s Context=%s Key=%s",
                Path.c_str(),
                Context,
                Key);
            return false;
        }

        return true;
    }

    bool ParseCompareOperator(const FString& Text, EAnimCompareOperator& OutOperator)
    {
        if (Text == "==")
        {
            OutOperator = EAnimCompareOperator::Equal;
            return true;
        }
        if (Text == "!=")
        {
            OutOperator = EAnimCompareOperator::NotEqual;
            return true;
        }
        if (Text == ">")
        {
            OutOperator = EAnimCompareOperator::Greater;
            return true;
        }
        if (Text == ">=")
        {
            OutOperator = EAnimCompareOperator::GreaterEqual;
            return true;
        }
        if (Text == "<")
        {
            OutOperator = EAnimCompareOperator::Less;
            return true;
        }
        if (Text == "<=")
        {
            OutOperator = EAnimCompareOperator::LessEqual;
            return true;
        }

        return false;
    }

    const char* ToCompareOperatorString(EAnimCompareOperator Operator)
    {
        switch (Operator)
        {
        case EAnimCompareOperator::Equal:
            return "==";
        case EAnimCompareOperator::NotEqual:
            return "!=";
        case EAnimCompareOperator::Greater:
            return ">";
        case EAnimCompareOperator::GreaterEqual:
            return ">=";
        case EAnimCompareOperator::Less:
            return "<";
        case EAnimCompareOperator::LessEqual:
            return "<=";
        default:
            return "==";
        }
    }

    bool ParseConditionType(const FString& Text, EAnimConditionType& OutType)
    {
        const FString LowerText = ToLowerString(Text);
        if (LowerText.empty() || LowerText == "none")
        {
            OutType = EAnimConditionType::None;
            return true;
        }
        if (LowerText == "bool")
        {
            OutType = EAnimConditionType::Bool;
            return true;
        }
        if (LowerText == "float")
        {
            OutType = EAnimConditionType::Float;
            return true;
        }
        if (LowerText == "luafunction" || LowerText == "lua_function")
        {
            OutType = EAnimConditionType::LuaFunction;
            return true;
        }

        return false;
    }

    const char* ToConditionTypeString(EAnimConditionType Type)
    {
        switch (Type)
        {
        case EAnimConditionType::Bool:
            return "Bool";
        case EAnimConditionType::Float:
            return "Float";
        case EAnimConditionType::LuaFunction:
            return "LuaFunction";
        case EAnimConditionType::None:
        default:
            return "None";
        }
    }

    bool IsValidCompareOperator(EAnimCompareOperator Operator)
    {
        switch (Operator)
        {
        case EAnimCompareOperator::Equal:
        case EAnimCompareOperator::NotEqual:
        case EAnimCompareOperator::Greater:
        case EAnimCompareOperator::GreaterEqual:
        case EAnimCompareOperator::Less:
        case EAnimCompareOperator::LessEqual:
            return true;
        default:
            return false;
        }
    }

    bool ParseCondition(
        json::JSON& TransitionNode,
        FAnimTransitionConditionDesc& OutCondition,
        const FString& Path,
        const char* Context)
    {
        if (!TransitionNode.hasKey("Condition"))
        {
            return true;
        }

        json::JSON& ConditionNode = TransitionNode["Condition"];
        if (!IsJsonObject(ConditionNode))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Condition must be an object. Path=%s Context=%s",
                Path.c_str(),
                Context);
            return false;
        }

        FString TypeText = "None";
        if (ConditionNode.hasKey("Type") && !ReadStringValue(ConditionNode["Type"], TypeText))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid condition type. Path=%s Context=%s",
                Path.c_str(),
                Context);
            return false;
        }

        if (!ParseConditionType(TypeText, OutCondition.Type))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Unknown condition type. Path=%s Context=%s Type=%s",
                Path.c_str(),
                Context,
                TypeText.c_str());
            return false;
        }

        if (OutCondition.Type == EAnimConditionType::None)
        {
            return true;
        }

        if (OutCondition.Type == EAnimConditionType::LuaFunction)
        {
            FString FunctionName;
            if (!ReadRequiredString(ConditionNode, "Function", Path, Context, FunctionName))
            {
                return false;
            }
            OutCondition.LuaFunctionName = FName(FunctionName);
            return true;
        }

        FString VariableName;
        if (!ReadRequiredString(ConditionNode, "Variable", Path, Context, VariableName))
        {
            return false;
        }
        OutCondition.VariableName = FName(VariableName);

        if (OutCondition.Type == EAnimConditionType::Float)
        {
            FString OperatorText;
            if (!ReadRequiredString(ConditionNode, "Operator", Path, Context, OperatorText))
            {
                return false;
            }
            if (!ParseCompareOperator(OperatorText, OutCondition.Operator))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Unknown compare operator. Path=%s Context=%s Operator=%s",
                    Path.c_str(),
                    Context,
                    OperatorText.c_str());
                return false;
            }

            return ReadRequiredFloat(ConditionNode, "Value", Path, Context, OutCondition.FloatValue);
        }

        if (OutCondition.Type == EAnimConditionType::Bool)
        {
            return ReadRequiredBool(ConditionNode, "Value", Path, Context, OutCondition.BoolValue);
        }

        return false;
    }

    bool ParseEditorPosition(
        json::JSON& StateNode,
        FAnimStateDesc& State,
        const FString& Path,
        const char* Context)
    {
        if (!StateNode.hasKey("Position"))
        {
            return true;
        }

        json::JSON& PositionNode = StateNode["Position"];
        if (!IsJsonArray(PositionNode) || PositionNode.length() != 2)
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Position must be [x, y]. Path=%s Context=%s",
                Path.c_str(),
                Context);
            return false;
        }

        float X = 0.0f;
        float Y = 0.0f;
        if (!ReadFloatValue(PositionNode[0], X) || !ReadFloatValue(PositionNode[1], Y))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Position contains invalid number. Path=%s Context=%s",
                Path.c_str(),
                Context);
            return false;
        }

        State.EditorPosition = FVector2(X, Y);
        return true;
    }

    bool ParseState(
        json::JSON& StateNode,
        FAnimStateDesc& OutState,
        const FString& Path,
        int32 StateIndex,
        bool bAllowDraft)
    {
        const FString Context = "State[" + std::to_string(StateIndex) + "]";
        if (!IsJsonObject(StateNode))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] State must be an object. Path=%s Context=%s",
                Path.c_str(),
                Context.c_str());
            return false;
        }

        FString StateName;
        if (!ReadRequiredInt(StateNode, "Id", Path, Context.c_str(), OutState.Id) ||
            !ReadRequiredString(StateNode, "Name", Path, Context.c_str(), StateName))
        {
            return false;
        }
        OutState.Name = FName(StateName);

        if (!StateNode.hasKey("Animation") || !IsJsonObject(StateNode["Animation"]))
        {
            if (bAllowDraft)
            {
                OutState.Animation.SourceFbxPath.clear();
                OutState.Animation.AnimStackName.clear();
                if (!ReadOptionalBool(StateNode, "Loop", Path, Context.c_str(), OutState.bLooping) ||
                    !ReadOptionalFloat(StateNode, "PlayRate", Path, Context.c_str(), OutState.PlayRate) ||
                    !ParseEditorPosition(StateNode, OutState, Path, Context.c_str()))
                {
                    return false;
                }
                return true;
            }

            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Missing or invalid animation reference. Path=%s Context=%s",
                Path.c_str(),
                Context.c_str());
            return false;
        }

        json::JSON& AnimationNode = StateNode["Animation"];
        const bool bReadAnimation = bAllowDraft
            ? (ReadOptionalString(AnimationNode, "SourceFbx", Path, Context.c_str(), OutState.Animation.SourceFbxPath) &&
                ReadOptionalString(AnimationNode, "AnimStack", Path, Context.c_str(), OutState.Animation.AnimStackName))
            : (ReadRequiredString(AnimationNode, "SourceFbx", Path, Context.c_str(), OutState.Animation.SourceFbxPath) &&
                ReadRequiredString(AnimationNode, "AnimStack", Path, Context.c_str(), OutState.Animation.AnimStackName));
        if (!bReadAnimation)
        {
            return false;
        }

        OutState.Animation.SourceFbxPath = FPaths::Normalize(OutState.Animation.SourceFbxPath);
        if (!ReadOptionalBool(StateNode, "Loop", Path, Context.c_str(), OutState.bLooping) ||
            !ReadOptionalFloat(StateNode, "PlayRate", Path, Context.c_str(), OutState.PlayRate) ||
            !ParseEditorPosition(StateNode, OutState, Path, Context.c_str()))
        {
            return false;
        }

        return true;
    }

    bool ParseTransition(
        json::JSON& TransitionNode,
        FAnimTransitionDesc& OutTransition,
        const FString& Path,
        int32 TransitionIndex)
    {
        const FString Context = "Transition[" + std::to_string(TransitionIndex) + "]";
        if (!IsJsonObject(TransitionNode))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Transition must be an object. Path=%s Context=%s",
                Path.c_str(),
                Context.c_str());
            return false;
        }

        FString FromState;
        FString ToState;
        if (!ReadRequiredInt(TransitionNode, "Id", Path, Context.c_str(), OutTransition.Id) ||
            !ReadRequiredString(TransitionNode, "From", Path, Context.c_str(), FromState) ||
            !ReadRequiredString(TransitionNode, "To", Path, Context.c_str(), ToState))
        {
            return false;
        }

        OutTransition.FromState = FName(FromState);
        OutTransition.ToState = FName(ToState);

        if (!ReadOptionalFloat(TransitionNode, "BlendTime", Path, Context.c_str(), OutTransition.BlendTime) ||
            !ReadOptionalInt(TransitionNode, "Priority", Path, Context.c_str(), OutTransition.Priority) ||
            !ReadOptionalBool(TransitionNode, "CanInterrupt", Path, Context.c_str(), OutTransition.bCanInterrupt) ||
            !ReadOptionalBool(TransitionNode, "CanBeInterrupted", Path, Context.c_str(), OutTransition.bCanBeInterrupted) ||
            !ParseCondition(TransitionNode, OutTransition.Condition, Path, Context.c_str()))
        {
            return false;
        }

        return true;
    }

    bool ValidateCondition(
        const FAnimTransitionConditionDesc& Condition,
        const FString& Path,
        const FString& Context)
    {
        switch (Condition.Type)
        {
        case EAnimConditionType::None:
            return true;
        case EAnimConditionType::Bool:
            if (!IsValidAnimStateName(Condition.VariableName))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Bool condition variable is invalid. Path=%s Context=%s",
                    Path.c_str(),
                    Context.c_str());
                return false;
            }
            return true;
        case EAnimConditionType::Float:
            if (!IsValidAnimStateName(Condition.VariableName) || !IsValidCompareOperator(Condition.Operator))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Float condition is invalid. Path=%s Context=%s",
                    Path.c_str(),
                    Context.c_str());
                return false;
            }
            return true;
        case EAnimConditionType::LuaFunction:
            if (!IsValidAnimStateName(Condition.LuaFunctionName))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Lua function condition name is invalid. Path=%s Context=%s",
                    Path.c_str(),
                    Context.c_str());
                return false;
            }
            return true;
        default:
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Unknown condition enum. Path=%s Context=%s",
                Path.c_str(),
                Context.c_str());
            return false;
        }
    }

    bool HasDuplicateStateName(const FAnimStateMachineDesc& Desc, int32 StateIndex)
    {
        const FAnimStateDesc& State = Desc.States[StateIndex];
        for (int32 OtherIndex = 0; OtherIndex < StateIndex; ++OtherIndex)
        {
            if (Desc.States[OtherIndex].Name == State.Name)
            {
                return true;
            }
        }
        return false;
    }

    bool HasDuplicateStateId(const FAnimStateMachineDesc& Desc, int32 StateIndex)
    {
        const FAnimStateDesc& State = Desc.States[StateIndex];
        for (int32 OtherIndex = 0; OtherIndex < StateIndex; ++OtherIndex)
        {
            if (Desc.States[OtherIndex].Id == State.Id)
            {
                return true;
            }
        }
        return false;
    }

    bool HasDuplicateTransitionId(const FAnimStateMachineDesc& Desc, int32 TransitionIndex)
    {
        const FAnimTransitionDesc& Transition = Desc.Transitions[TransitionIndex];
        for (int32 OtherIndex = 0; OtherIndex < TransitionIndex; ++OtherIndex)
        {
            if (Desc.Transitions[OtherIndex].Id == Transition.Id)
            {
                return true;
            }
        }
        return false;
    }

	/**
	 * @brief 파싱이 끝난 뒤 state 및 transition 참조 관계를 한 번에 검증해 잘못된 asset을 초기에 걸러냄
	 */
    bool ValidateDesc(const FAnimStateMachineDesc& Desc, const FString& Path)
    {
        if (Desc.AssetType != "AnimationStateMachine")
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Invalid AssetType. Path=%s AssetType=%s",
                Path.c_str(),
                Desc.AssetType.c_str());
            return false;
        }

        if (Desc.Version != AnimStateMachineAssetVersion)
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] Unsupported Version. Path=%s Version=%d",
                Path.c_str(),
                Desc.Version);
            return false;
        }

        if (!IsValidAnimStateName(Desc.EntryState))
        {
            UE_LOG_ERROR("[AnimStateMachineLoader] EntryState is invalid. Path=%s", Path.c_str());
            return false;
        }

        if (Desc.States.empty())
        {
            UE_LOG_ERROR("[AnimStateMachineLoader] States is empty. Path=%s", Path.c_str());
            return false;
        }

        for (int32 StateIndex = 0; StateIndex < static_cast<int32>(Desc.States.size()); ++StateIndex)
        {
            const FAnimStateDesc& State = Desc.States[StateIndex];
            if (State.Id < 0 || !IsValidAnimStateName(State.Name) || FAnimStateMachineDesc::IsAnyStateName(State.Name))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] State has invalid id or name. Path=%s StateIndex=%d Id=%d Name=%s",
                    Path.c_str(),
                    StateIndex,
                    State.Id,
                    State.Name.ToString().c_str());
                return false;
            }

            if (State.Animation.SourceFbxPath.empty() || State.Animation.AnimStackName.empty())
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] State animation reference is invalid. Path=%s State=%s",
                    Path.c_str(),
                    State.Name.ToString().c_str());
                return false;
            }

            if (HasDuplicateStateId(Desc, StateIndex) || HasDuplicateStateName(Desc, StateIndex))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Duplicate state id or name. Path=%s State=%s Id=%d",
                    Path.c_str(),
                    State.Name.ToString().c_str(),
                    State.Id);
                return false;
            }
        }

        if (!Desc.HasState(Desc.EntryState))
        {
            UE_LOG_ERROR(
                "[AnimStateMachineLoader] EntryState does not exist in States. Path=%s EntryState=%s",
                Path.c_str(),
                Desc.EntryState.ToString().c_str());
            return false;
        }

        for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(Desc.Transitions.size()); ++TransitionIndex)
        {
            const FAnimTransitionDesc& Transition = Desc.Transitions[TransitionIndex];
            const FString Context = "Transition[" + std::to_string(TransitionIndex) + "]";
            if (Transition.Id < 0 ||
                !IsValidAnimStateName(Transition.FromState) ||
                !IsValidAnimStateName(Transition.ToState))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Transition has invalid id or state. Path=%s Context=%s Id=%d From=%s To=%s",
                    Path.c_str(),
                    Context.c_str(),
                    Transition.Id,
                    Transition.FromState.ToString().c_str(),
                    Transition.ToState.ToString().c_str());
                return false;
            }

            if (HasDuplicateTransitionId(Desc, TransitionIndex))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Duplicate transition id. Path=%s Context=%s Id=%d",
                    Path.c_str(),
                    Context.c_str(),
                    Transition.Id);
                return false;
            }

            if (!FAnimStateMachineDesc::IsAnyStateName(Transition.FromState) && !Desc.HasState(Transition.FromState))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Transition source state does not exist. Path=%s Context=%s From=%s",
                    Path.c_str(),
                    Context.c_str(),
                    Transition.FromState.ToString().c_str());
                return false;
            }

            if (!Desc.HasState(Transition.ToState))
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Transition target state does not exist. Path=%s Context=%s To=%s",
                    Path.c_str(),
                    Context.c_str(),
                    Transition.ToState.ToString().c_str());
                return false;
            }

            if (Transition.BlendTime < 0.0f)
            {
                UE_LOG_ERROR(
                    "[AnimStateMachineLoader] Transition BlendTime must not be negative. Path=%s Context=%s Id=%d From=%s To=%s BlendTime=%.3f",
                    Path.c_str(),
                    Context.c_str(),
                    Transition.Id,
                    Transition.FromState.ToString().c_str(),
                    Transition.ToState.ToString().c_str(),
                    Transition.BlendTime);
                return false;
            }

            if (!ValidateCondition(Transition.Condition, Path, Context))
            {
                return false;
            }
        }

        return true;
    }

    json::JSON MakeConditionJson(const FAnimTransitionConditionDesc& Condition)
    {
        json::JSON ConditionNode = json::JSON::Make(json::JSON::Class::Object);
        ConditionNode["Type"] = ToConditionTypeString(Condition.Type);

        switch (Condition.Type)
        {
        case EAnimConditionType::Bool:
            ConditionNode["Variable"] = Condition.VariableName.ToString();
            ConditionNode["Value"] = Condition.BoolValue;
            break;
        case EAnimConditionType::Float:
            ConditionNode["Variable"] = Condition.VariableName.ToString();
            ConditionNode["Operator"] = ToCompareOperatorString(Condition.Operator);
            ConditionNode["Value"] = Condition.FloatValue;
            break;
        case EAnimConditionType::LuaFunction:
            ConditionNode["Function"] = Condition.LuaFunctionName.ToString();
            break;
        case EAnimConditionType::None:
        default:
            break;
        }

        return ConditionNode;
    }

    json::JSON MakeDescJson(const FAnimStateMachineDesc& Desc)
    {
        json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
        Root["AssetType"] = Desc.AssetType;
        Root["Version"] = Desc.Version;
        Root["EntryState"] = Desc.EntryState.ToString();

        json::JSON StatesNode = json::JSON::Make(json::JSON::Class::Array);
        for (const FAnimStateDesc& State : Desc.States)
        {
            json::JSON StateNode = json::JSON::Make(json::JSON::Class::Object);
            StateNode["Id"] = State.Id;
            StateNode["Name"] = State.Name.ToString();

            json::JSON AnimationNode = json::JSON::Make(json::JSON::Class::Object);
            AnimationNode["SourceFbx"] = State.Animation.SourceFbxPath;
            AnimationNode["AnimStack"] = State.Animation.AnimStackName;
            StateNode["Animation"] = AnimationNode;

            StateNode["Loop"] = State.bLooping;
            StateNode["PlayRate"] = State.PlayRate;

            json::JSON PositionNode = json::JSON::Make(json::JSON::Class::Array);
            PositionNode.append(State.EditorPosition.X);
            PositionNode.append(State.EditorPosition.Y);
            StateNode["Position"] = PositionNode;

            StatesNode.append(StateNode);
        }
        Root["States"] = StatesNode;

        json::JSON TransitionsNode = json::JSON::Make(json::JSON::Class::Array);
        for (const FAnimTransitionDesc& Transition : Desc.Transitions)
        {
            json::JSON TransitionNode = json::JSON::Make(json::JSON::Class::Object);
            TransitionNode["Id"] = Transition.Id;
            TransitionNode["From"] = Transition.FromState.ToString();
            TransitionNode["To"] = Transition.ToState.ToString();
            TransitionNode["BlendTime"] = Transition.BlendTime;
            TransitionNode["Priority"] = Transition.Priority;
            TransitionNode["CanInterrupt"] = Transition.bCanInterrupt;
            TransitionNode["CanBeInterrupted"] = Transition.bCanBeInterrupted;
            TransitionNode["Condition"] = MakeConditionJson(Transition.Condition);
            TransitionsNode.append(TransitionNode);
        }
        Root["Transitions"] = TransitionsNode;

        return Root;
    }
}

UAnimStateMachine* FAnimStateMachineLoader::Load(const FString& Path) const
{
    FAnimStateMachineDesc Desc;
    if (!ReadDesc(Path, Desc))
    {
        return nullptr;
    }

    const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    UAnimStateMachine* StateMachine = UObjectManager::Get().CreateObject<UAnimStateMachine>();
    StateMachine->SetDesc(Desc);
    StateMachine->SetAssetPath(NormalizedPath);
    return StateMachine;
}

bool FAnimStateMachineLoader::Save(const FString& Path, const UAnimStateMachine* StateMachine) const
{
    if (!StateMachine)
    {
        return false;
    }

    return WriteDesc(Path, StateMachine->GetDesc());
}

bool FAnimStateMachineLoader::SupportsExtension(const FString& Extension) const
{
    const FString LowerExtension = ToLowerString(Extension);
    return LowerExtension == ".animsm" || LowerExtension == "animsm";
}

FString FAnimStateMachineLoader::GetLoaderName() const
{
    return "FAnimStateMachineLoader";
}

bool FAnimStateMachineLoader::LoadDescForEditor(const FString& Path, FAnimStateMachineDesc& OutDesc) const
{
    return ReadDescInternal(Path, OutDesc, false, true);
}

bool FAnimStateMachineLoader::SaveDescForEditor(const FString& Path, const FAnimStateMachineDesc& Desc) const
{
    return WriteDescInternal(Path, Desc, false);
}

bool FAnimStateMachineLoader::ValidateDescForRuntime(const FAnimStateMachineDesc& Desc, const FString& Path) const
{
    return ValidateDesc(Desc, Path.empty() ? FString("<editor>") : Path);
}

FString FAnimStateMachineLoader::BuildDescJsonForEditor(const FAnimStateMachineDesc& Desc) const
{
    return MakeDescJson(Desc).dump(4);
}

bool FAnimStateMachineLoader::ReadDesc(const FString& Path, FAnimStateMachineDesc& OutDesc) const
{
    return ReadDescInternal(Path, OutDesc, true, false);
}

bool FAnimStateMachineLoader::ReadDescInternal(
    const FString& Path,
    FAnimStateMachineDesc& OutDesc,
    bool bValidateRuntime,
    bool bAllowDraft) const
{
    const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    if (NormalizedPath.empty())
    {
        return false;
    }

    std::ifstream StateMachineFile(FPaths::ToWide(NormalizedPath));
    if (!StateMachineFile.is_open())
    {
        UE_LOG_ERROR("[AnimStateMachineLoader] Failed to open asset: %s", NormalizedPath.c_str());
        return false;
    }

    FString FileContent((std::istreambuf_iterator<char>(StateMachineFile)), std::istreambuf_iterator<char>());
    if (FileContent.empty())
    {
        UE_LOG_ERROR("[AnimStateMachineLoader] Empty asset file: %s", NormalizedPath.c_str());
        return false;
    }

    json::JSON Root;
    try
    {
        Root = json::JSON::Load(FileContent);
    }
    catch (const std::exception& Exception)
    {
        UE_LOG_ERROR(
            "[AnimStateMachineLoader] Failed to parse json. Path=%s Error=%s",
            NormalizedPath.c_str(),
            Exception.what());
        return false;
    }

    if (!IsJsonObject(Root))
    {
        UE_LOG_ERROR("[AnimStateMachineLoader] Root must be an object: %s", NormalizedPath.c_str());
        return false;
    }

    FAnimStateMachineDesc Desc;
    FString EntryStateName;
    if (!ReadRequiredString(Root, "AssetType", NormalizedPath, "Root", Desc.AssetType) ||
        !ReadRequiredInt(Root, "Version", NormalizedPath, "Root", Desc.Version))
    {
        return false;
    }

    const bool bReadEntryState = bAllowDraft
        ? ReadOptionalString(Root, "EntryState", NormalizedPath, "Root", EntryStateName)
        : ReadRequiredString(Root, "EntryState", NormalizedPath, "Root", EntryStateName);
    if (!bReadEntryState)
    {
        return false;
    }
    Desc.EntryState = FName(EntryStateName);

    if (!Root.hasKey("States") || !IsJsonArray(Root["States"]))
    {
        if (bAllowDraft && !Root.hasKey("States"))
        {
            OutDesc = Desc;
            return true;
        }

        UE_LOG_ERROR("[AnimStateMachineLoader] States must be an array. Path=%s", NormalizedPath.c_str());
        return false;
    }

    json::JSON& StatesNode = Root["States"];
    Desc.States.reserve(StatesNode.length() > 0 ? static_cast<size_t>(StatesNode.length()) : 0);
    for (int32 StateIndex = 0; StateIndex < StatesNode.length(); ++StateIndex)
    {
        FAnimStateDesc State;
        if (!ParseState(StatesNode[static_cast<unsigned>(StateIndex)], State, NormalizedPath, StateIndex, bAllowDraft))
        {
            return false;
        }
        Desc.States.push_back(State);
    }

    if (Root.hasKey("Transitions"))
    {
        if (!IsJsonArray(Root["Transitions"]))
        {
            UE_LOG_ERROR("[AnimStateMachineLoader] Transitions must be an array. Path=%s", NormalizedPath.c_str());
            return false;
        }

        json::JSON& TransitionsNode = Root["Transitions"];
        Desc.Transitions.reserve(TransitionsNode.length() > 0 ? static_cast<size_t>(TransitionsNode.length()) : 0);
        for (int32 TransitionIndex = 0; TransitionIndex < TransitionsNode.length(); ++TransitionIndex)
        {
            FAnimTransitionDesc Transition;
            if (!ParseTransition(
                    TransitionsNode[static_cast<unsigned>(TransitionIndex)],
                    Transition,
                    NormalizedPath,
                    TransitionIndex))
            {
                return false;
            }
            Desc.Transitions.push_back(Transition);
        }
    }

    if (bValidateRuntime && !ValidateDesc(Desc, NormalizedPath))
    {
        return false;
    }

    OutDesc = Desc;
    return true;
}

bool FAnimStateMachineLoader::WriteDesc(const FString& Path, const FAnimStateMachineDesc& Desc) const
{
    return WriteDescInternal(Path, Desc, true);
}

bool FAnimStateMachineLoader::WriteDescInternal(
    const FString& Path,
    const FAnimStateMachineDesc& Desc,
    bool bValidateRuntime) const
{
    const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    if (NormalizedPath.empty())
    {
        return false;
    }

    if (bValidateRuntime && !ValidateDesc(Desc, NormalizedPath))
    {
        return false;
    }

    const json::JSON Root = MakeDescJson(Desc);

    std::error_code ErrorCode;
    std::filesystem::path FilePath(FPaths::ToWide(NormalizedPath));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

    std::ofstream OutFile(FilePath);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[AnimStateMachineLoader] Failed to open asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    OutFile << Root.dump(4);
    return true;
}
