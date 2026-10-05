#include "Runtime/Script/ScriptManager.h"

#include "Animation/ActorSequence.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/StateMachineAnimInstance.h"
#include "Asset/CurveFloatAsset.h"
#include "Runtime/Script/ScriptComponent.h"
#include "Runtime/Script/ScriptUtils.h"

void FScriptManager::BindAnimationTypes()
{
	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCurveFloatAsset, "CurveFloatAsset", UObject)
	LUA_METHOD(Evaluate, Evaluate);
	LUA_METHOD(GetAssetPath, GetAssetPath);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorSequence, "ActorSequence", UObject)
	LUA_FIELD(StartTime, StartTime);
	LUA_FIELD(Duration, Duration);
	LUA_FIELD(Loop, bLoop);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorSequencePlayer, "ActorSequencePlayer", UObject)
	LUA_METHOD(Play, Play);
	LUA_METHOD(Pause, Pause);
	LUA_METHOD(Stop, Stop);
	LUA_METHOD(SetCurrentTime, SetCurrentTime);
	LUA_METHOD(GetCurrentTime, GetCurrentTime);
	LUA_METHOD(IsPlaying, IsPlaying);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR(GLuaState, FLuaTimeline, "LuaTimeline")
	LUA_METHOD(Play, Play);
	LUA_METHOD(Pause, Pause);
	LUA_METHOD(Stop, Stop);
	LUA_METHOD(Tick, Tick);
	LUA_METHOD(SetPlayRate, SetPlayRate);
	LUA_METHOD(SetLoop, SetLoop);
	LUA_METHOD(SetCurrentTime, SetCurrentTime);
	LUA_METHOD(GetCurrentTime, GetCurrentTime);
	LUA_METHOD(AddFloatTrack, AddFloatTrack);
	LUA_METHOD(ClearTracks, ClearTracks);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAnimInstance, "AnimInstance", UObject)
	LUA_METHOD(GetCurrentTime, GetCurrentTime);
	LUA_METHOD(GetPreviousTime, GetPreviousTime);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAnimSingleNodeInstance, "AnimSingleNodeInstance", UAnimInstance, UObject)
	LUA_METHOD(Play, Play);
	LUA_METHOD(Stop, Stop);
	LUA_METHOD(Pause, Pause);
	LUA_METHOD(SetPlayRate, SetPlayRate);
	LUA_METHOD(SetLooping, SetLooping);
	LUA_METHOD(SetPosition, SetPosition);
	LUA_METHOD(IsPlaying, IsPlaying);
	LUA_METHOD(IsLooping, IsLooping);
	LUA_METHOD(GetPlayRate, GetPlayRate);
	LUA_METHOD(GetLength, GetLength);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAnimationStateMachine, "AnimationStateMachine", UObject)
	LUA_METHOD(AddStateFromPath, AddStateFromPath);
	LUA_SET(AddTransitionByName, [](UAnimationStateMachine& Self, const FString& FromState, const FString& ToState, float BlendTime, sol::function Condition)
	{
		if (!Condition.valid())
		{
			UE_LOG_WARNING("[AnimationStateMachine] Ignored invalid Lua transition condition: %s -> %s",
				FromState.c_str(),
				ToState.c_str());
			return;
		}

		sol::protected_function ProtectedCondition = Condition;
		Self.AddTransition(
			FName(FromState.c_str()),
			FName(ToState.c_str()),
			BlendTime,
			[ProtectedCondition]() mutable -> bool
			{
				sol::protected_function_result Result = ProtectedCondition();
				if (!Result.valid())
				{
					sol::error Error = Result;
					UE_LOG_ERROR("[AnimationStateMachine] Lua transition condition failed: %s", Error.what());
					return false;
				}

				sol::object ResultObject = Result;
				if (!ResultObject.is<bool>())
				{
					return false;
				}

				return ResultObject.as<bool>();
			});
	});
	LUA_METHOD(ClearTransitions, ClearTransitions);
	LUA_METHOD(SetEntryStateByName, SetEntryStateByName);
	LUA_METHOD(SetStateByName, SetStateByName);
	LUA_METHOD(GetCurrentStateName, GetCurrentStateName);
	LUA_METHOD(GetNextStateName, GetNextStateName);
	LUA_METHOD(IsBlending, IsBlending);
	LUA_END_TYPE();

	LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UStateMachineAnimInstance, "StateMachineAnimInstance", UAnimInstance, UObject)
	LUA_METHOD(SetStateMachine, SetStateMachine);
	LUA_END_TYPE();
}
