#include "Runtime/Script/LuaReflectionBridge.h"

#include "Core/CoreMinimal.h"
#include "Object/Class.h"
#include "Object/Function.h"
#include "Object/Object.h"
#include "Object/Property.h"

#include <cmath>
#include <cstring>

namespace
{
	bool IsNilObject(const sol::object& Value);
	bool IsStructType(const FProperty& Property, const char* TypeName);
	bool ReadLuaNumber(const sol::object& Value, float& OutValue);
	bool ReadLuaInteger(const sol::object& Value, int32& OutValue);
	bool TryReadFloatField(const sol::table& Table, const char* UpperName, const char* LowerName, int32 Index, float& OutValue);
	bool ReadRequiredFloatField(const sol::table& Table, const char* UpperName, const char* LowerName, int32 Index, float& OutValue);
	void WriteEnumInteger(void* ValuePtr, uint8 Size, int64 Value);
	int64 ReadEnumInteger(const void* ValuePtr, uint8 Size);
	bool WriteLuaEnum(sol::object Value, const FProperty& Property, void* ValuePtr);
	sol::object ReadEnumToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr);
	bool WriteLuaObjectPtr(sol::object Value, const FProperty& Property, void* ValuePtr);
	sol::object ReadObjectPtrToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr);
	bool WriteLuaSoftObjectPtr(sol::object Value, const FProperty& Property, void* ValuePtr);
	sol::object ReadSoftObjectPtrToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr);
	bool WriteLuaArray(sol::object Value, const FProperty& Property, void* ValuePtr);
	sol::object ReadArrayToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr);
	bool WriteLuaStruct(sol::object Value, const FProperty& Property, void* ValuePtr);
	sol::object ReadStructToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr);
	bool WriteLuaVector(sol::object Value, FVector& OutVector);
	bool WriteLuaVector2(sol::object Value, FVector2& OutVector);
	bool WriteLuaVector4(sol::object Value, FVector4& OutVector);
	bool WriteLuaColor(sol::object Value, FColor& OutColor);
	bool WriteLuaQuat(sol::object Value, FQuat& OutQuat);
	bool WriteLuaGuid(sol::object Value, FGuid& OutGuid);
	sol::object MakeVector2Table(sol::this_state State, const FVector2& Value);
	sol::object MakeVector4Table(sol::this_state State, const FVector4& Value);
	sol::object MakeColorTable(sol::this_state State, const FColor& Value);
}

bool FLuaPropertyValueCodec::WriteLuaValueToProperty(sol::object Value, const FProperty& Property, void* ValuePtr)
{
	if (!ValuePtr)
	{
		return false;
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		if (!Value.is<bool>())
		{
			return false;
		}
		*static_cast<bool*>(ValuePtr) = Value.as<bool>();
		return true;
	case EPropertyType::Int:
	{
		int32 IntValue = 0;
		if (!ReadLuaInteger(Value, IntValue))
		{
			return false;
		}
		*static_cast<int32*>(ValuePtr) = IntValue;
		return true;
	}
	case EPropertyType::Float:
	{
		float FloatValue = 0.0f;
		if (!ReadLuaNumber(Value, FloatValue))
		{
			return false;
		}
		*static_cast<float*>(ValuePtr) = FloatValue;
		return true;
	}
	case EPropertyType::String:
		if (!Value.is<FString>())
		{
			return false;
		}
		*static_cast<FString*>(ValuePtr) = Value.as<FString>();
		return true;
	case EPropertyType::Name:
		if (!Value.is<FString>())
		{
			return false;
		}
		*static_cast<FName*>(ValuePtr) = FName(Value.as<FString>());
		return true;
	case EPropertyType::Enum:
		return WriteLuaEnum(Value, Property, ValuePtr);
	case EPropertyType::ObjectPtr:
		return WriteLuaObjectPtr(Value, Property, ValuePtr);
	case EPropertyType::SoftObjectPtr:
		return WriteLuaSoftObjectPtr(Value, Property, ValuePtr);
	case EPropertyType::Array:
		return WriteLuaArray(Value, Property, ValuePtr);
	case EPropertyType::Struct:
		return WriteLuaStruct(Value, Property, ValuePtr);
	case EPropertyType::Unknown:
	default:
		return false;
	}
}

sol::object FLuaPropertyValueCodec::ReadPropertyToLuaValue(sol::this_state State, const FProperty& Property, const void* ValuePtr)
{
	if (!ValuePtr)
	{
		return sol::make_object(State, sol::nil);
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		return sol::make_object(State, *static_cast<const bool*>(ValuePtr));
	case EPropertyType::Int:
		return sol::make_object(State, *static_cast<const int32*>(ValuePtr));
	case EPropertyType::Float:
		return sol::make_object(State, *static_cast<const float*>(ValuePtr));
	case EPropertyType::String:
		return sol::make_object(State, *static_cast<const FString*>(ValuePtr));
	case EPropertyType::Name:
		return sol::make_object(State, static_cast<const FName*>(ValuePtr)->ToString());
	case EPropertyType::Enum:
		return ReadEnumToLua(State, Property, ValuePtr);
	case EPropertyType::ObjectPtr:
		return ReadObjectPtrToLua(State, Property, ValuePtr);
	case EPropertyType::SoftObjectPtr:
		return ReadSoftObjectPtrToLua(State, Property, ValuePtr);
	case EPropertyType::Array:
		return ReadArrayToLua(State, Property, ValuePtr);
	case EPropertyType::Struct:
		return ReadStructToLua(State, Property, ValuePtr);
	case EPropertyType::Unknown:
	default:
		return sol::make_object(State, sol::nil);
	}
}

sol::object FLuaReflectionFunctionBridge::CallByName(sol::this_state State, UObject* Object, const FString& FunctionName, sol::variadic_args Args)
{
	if (!Object)
	{
		UE_LOG_ERROR("[LuaFunction] Cannot call '%s' on a null object", FunctionName.c_str());
		return sol::make_object(State, sol::nil);
	}

	UClass* Class = Object->GetClass();
	const UFunction* Function = Class ? Class->FindFunction(FunctionName.c_str()) : nullptr;
	return CallFunction(State, Object, Function, Args);
}

sol::object FLuaReflectionFunctionBridge::CallFunction(sol::this_state State, UObject* Object, const UFunction* Function, sol::variadic_args Args)
{
	if (!Object)
	{
		UE_LOG_ERROR("[LuaFunction] Cannot call a reflected function on a null object");
		return sol::make_object(State, sol::nil);
	}

	if (!Function)
	{
		UE_LOG_ERROR("[LuaFunction] Reflected function not found on %s", Object->GetClassName());
		return sol::make_object(State, sol::nil);
	}

	if (!Function->HasAnyFunctionFlags(EFunctionFlags::LuaCallable))
	{
		UE_LOG_ERROR("[LuaFunction] Function %s is not LuaCallable", Function->GetName());
		return sol::make_object(State, sol::nil);
	}

	TArray<const FProperty*> InputParams;
	for (const FProperty& Property : Function->GetProperties())
	{
		if (HasPropertyFlag(Property.Flags, EPropertyFlags::Parm) && !HasPropertyFlag(Property.Flags, EPropertyFlags::ReturnParm))
		{
			InputParams.push_back(&Property);
		}
	}

	if (Args.size() != InputParams.size())
	{
		UE_LOG_ERROR(
			"[LuaFunction] Function %s expects %zu args but received %zu",
			Function->GetName(),
			InputParams.size(),
			Args.size());
		return sol::make_object(State, sol::nil);
	}

	FScopedFunctionParams Params(Function);
	void* ParamsMemory = Params.GetMemory();

	for (size_t Index = 0; Index < InputParams.size(); ++Index)
	{
		const FProperty* ParamProperty = InputParams[Index];
		if (!ParamProperty)
		{
			return sol::make_object(State, sol::nil);
		}

		void* ValuePtr = reinterpret_cast<uint8*>(ParamsMemory) + ParamProperty->Offset;
		if (!FLuaPropertyValueCodec::WriteLuaValueToProperty(Args.get<sol::object>(static_cast<sol::variadic_args::difference_type>(Index)), *ParamProperty, ValuePtr))
		{
			UE_LOG_ERROR("[LuaFunction] Failed to convert argument %zu for %s", Index + 1, Function->GetName());
			return sol::make_object(State, sol::nil);
		}
	}

	Object->ProcessEvent(const_cast<UFunction*>(Function), ParamsMemory);

	const FProperty* ReturnProperty = Function->GetReturnProperty();
	if (!ReturnProperty)
	{
		return sol::make_object(State, sol::nil);
	}

	const void* ReturnPtr = reinterpret_cast<const uint8*>(ParamsMemory) + ReturnProperty->Offset;
	return FLuaPropertyValueCodec::ReadPropertyToLuaValue(State, *ReturnProperty, ReturnPtr);
}

namespace
{
	bool IsNilObject(const sol::object& Value)
	{
		return !Value.valid() || Value == sol::nil;
	}

	bool IsStructType(const FProperty& Property, const char* TypeName)
	{
		if (Property.Type != EPropertyType::Struct || !TypeName)
		{
			return false;
		}

		if (Property.EditorHint && std::strcmp(Property.EditorHint, TypeName) == 0)
		{
			return true;
		}

		return Property.ScriptStruct && Property.ScriptStruct->GetName() && std::strcmp(Property.ScriptStruct->GetName(), TypeName) == 0;
	}

	bool ReadLuaNumber(const sol::object& Value, float& OutValue)
	{
		if (!Value.valid() || (!Value.is<double>() && !Value.is<float>() && !Value.is<int32>()))
		{
			return false;
		}

		OutValue = Value.as<float>();
		return true;
	}

	bool ReadLuaInteger(const sol::object& Value, int32& OutValue)
	{
		if (!Value.valid() || (!Value.is<double>() && !Value.is<int32>()))
		{
			return false;
		}

		const double Number = Value.as<double>();
		if (std::floor(Number) != Number)
		{
			return false;
		}

		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool TryReadFloatField(const sol::table& Table, const char* UpperName, const char* LowerName, int32 Index, float& OutValue)
	{
		sol::object Value = Table[UpperName];
		if (!IsNilObject(Value) && ReadLuaNumber(Value, OutValue))
		{
			return true;
		}

		Value = Table[LowerName];
		if (!IsNilObject(Value) && ReadLuaNumber(Value, OutValue))
		{
			return true;
		}

		Value = Table[Index];
		return !IsNilObject(Value) && ReadLuaNumber(Value, OutValue);
	}

	bool ReadRequiredFloatField(const sol::table& Table, const char* UpperName, const char* LowerName, int32 Index, float& OutValue)
	{
		return TryReadFloatField(Table, UpperName, LowerName, Index, OutValue);
	}

	void WriteEnumInteger(void* ValuePtr, uint8 Size, int64 Value)
	{
		switch (Size)
		{
		case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(Value); break;
		case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(Value); break;
		case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(Value); break;
		case 8: *static_cast<int64*>(ValuePtr) = Value; break;
		default: break;
		}
	}

	int64 ReadEnumInteger(const void* ValuePtr, uint8 Size)
	{
		switch (Size)
		{
		case 1: return static_cast<int64>(*static_cast<const uint8*>(ValuePtr));
		case 2: return static_cast<int64>(*static_cast<const uint16*>(ValuePtr));
		case 4: return static_cast<int64>(*static_cast<const int32*>(ValuePtr));
		case 8: return *static_cast<const int64*>(ValuePtr);
		default: return 0;
		}
	}

	bool WriteLuaEnum(sol::object Value, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.EnumMeta || !ValuePtr)
		{
			return false;
		}

		if (Value.is<FString>())
		{
			const FString Text = Value.as<FString>();
			const FEnumValue* MatchedValue = nullptr;
			for (uint32 Index = 0; Index < Property.EnumMeta->GetCount(); ++Index)
			{
				const FEnumValue& Candidate = Property.EnumMeta->GetValues()[Index];
				const bool bNameMatch = Candidate.Name && Text == Candidate.Name;
				const bool bDisplayMatch = Candidate.DisplayName && Text == Candidate.DisplayName;
				if (bNameMatch || bDisplayMatch)
				{
					if (MatchedValue && bDisplayMatch)
					{
						return false;
					}
					MatchedValue = &Candidate;
				}
			}

			if (!MatchedValue)
			{
				return false;
			}

			WriteEnumInteger(ValuePtr, Property.EnumMeta->GetSize(), MatchedValue->Value);
			return true;
		}

		int32 NumericValue = 0;
		if (!ReadLuaInteger(Value, NumericValue))
		{
			return false;
		}

		WriteEnumInteger(ValuePtr, Property.EnumMeta->GetSize(), NumericValue);
		return true;
	}

	sol::object ReadEnumToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr)
	{
		if (!Property.EnumMeta || !ValuePtr)
		{
			return sol::make_object(State, sol::nil);
		}

		const int64 Value = ReadEnumInteger(ValuePtr, Property.EnumMeta->GetSize());
		for (uint32 Index = 0; Index < Property.EnumMeta->GetCount(); ++Index)
		{
			const FEnumValue& Candidate = Property.EnumMeta->GetValues()[Index];
			if (Candidate.Value == Value && Candidate.Name)
			{
				return sol::make_object(State, FString(Candidate.Name));
			}
		}

		return sol::make_object(State, static_cast<int32>(Value));
	}

	bool WriteLuaObjectPtr(sol::object Value, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.ObjectPtrOps)
		{
			return false;
		}

		if (IsNilObject(Value))
		{
			Property.ObjectPtrOps->SetObject(ValuePtr, nullptr);
			return true;
		}

		if (!Value.is<UObject*>())
		{
			return false;
		}

		UObject* Object = Value.as<UObject*>();
		if (Object && Property.ObjectClass && !Object->IsA(Property.ObjectClass))
		{
			return false;
		}

		Property.ObjectPtrOps->SetObject(ValuePtr, Object);
		return true;
	}

	sol::object ReadObjectPtrToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr)
	{
		if (!Property.ObjectPtrOps)
		{
			return sol::make_object(State, sol::nil);
		}

		UObject* Object = Property.ObjectPtrOps->GetObject(ValuePtr);
		return Object ? sol::make_object(State, Object) : sol::make_object(State, sol::nil);
	}

	bool WriteLuaSoftObjectPtr(sol::object Value, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.SoftObjectOps)
		{
			return false;
		}

		if (IsNilObject(Value))
		{
			Property.SoftObjectOps->SetPath(ValuePtr, FString());
			return true;
		}

		if (!Value.is<FString>())
		{
			return false;
		}

		Property.SoftObjectOps->SetPath(ValuePtr, Value.as<FString>());
		return true;
	}

	sol::object ReadSoftObjectPtrToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr)
	{
		if (!Property.SoftObjectOps)
		{
			return sol::make_object(State, sol::nil);
		}

		return sol::make_object(State, Property.SoftObjectOps->GetPath(ValuePtr));
	}

	bool WriteLuaArray(sol::object Value, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.ArrayOps || !Property.InnerProperty || !Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		const int32 Count = static_cast<int32>(Table.size());
		Property.ArrayOps->Resize(ValuePtr, Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
			sol::object ElementValue = Table.get<sol::object>(Index + 1);
			if (!ElementPtr || !FLuaPropertyValueCodec::WriteLuaValueToProperty(ElementValue, *Property.InnerProperty, ElementPtr))
			{
				return false;
			}
		}

		return true;
	}

	sol::object ReadArrayToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr)
	{
		sol::state_view Lua(State);
		sol::table Table = Lua.create_table();
		if (!Property.ArrayOps || !Property.InnerProperty)
		{
			return sol::make_object(State, Table);
		}

		const int32 Count = Property.ArrayOps->Num(ValuePtr);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
			Table[Index + 1] = FLuaPropertyValueCodec::ReadPropertyToLuaValue(State, *Property.InnerProperty, ElementPtr);
		}

		return sol::make_object(State, Table);
	}

	bool WriteLuaStruct(sol::object Value, const FProperty& Property, void* ValuePtr)
	{
		if (IsStructType(Property, "FVector"))
		{
			return WriteLuaVector(Value, *static_cast<FVector*>(ValuePtr));
		}
		if (IsStructType(Property, "FVector2"))
		{
			return WriteLuaVector2(Value, *static_cast<FVector2*>(ValuePtr));
		}
		if (IsStructType(Property, "FVector4"))
		{
			return WriteLuaVector4(Value, *static_cast<FVector4*>(ValuePtr));
		}
		if (IsStructType(Property, "FColor"))
		{
			return WriteLuaColor(Value, *static_cast<FColor*>(ValuePtr));
		}
		if (IsStructType(Property, "FQuat"))
		{
			return WriteLuaQuat(Value, *static_cast<FQuat*>(ValuePtr));
		}
		if (IsStructType(Property, "FGuid"))
		{
			return WriteLuaGuid(Value, *static_cast<FGuid*>(ValuePtr));
		}

		if (!Property.ScriptStruct || !Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		TArray<const FProperty*> ChildProperties;
		Property.ScriptStruct->GetAllProperties(ChildProperties);
		for (const FProperty* Child : ChildProperties)
		{
			if (!Child || !Child->Name)
			{
				continue;
			}

			sol::object ChildValue = Table[Child->Name];
			if (IsNilObject(ChildValue))
			{
				continue;
			}

			void* ChildPtr = reinterpret_cast<uint8*>(ValuePtr) + Child->Offset;
			if (!FLuaPropertyValueCodec::WriteLuaValueToProperty(ChildValue, *Child, ChildPtr))
			{
				return false;
			}
		}

		return true;
	}

	sol::object ReadStructToLua(sol::this_state State, const FProperty& Property, const void* ValuePtr)
	{
		if (IsStructType(Property, "FVector"))
		{
			return sol::make_object(State, *static_cast<const FVector*>(ValuePtr));
		}
		if (IsStructType(Property, "FVector2"))
		{
			return MakeVector2Table(State, *static_cast<const FVector2*>(ValuePtr));
		}
		if (IsStructType(Property, "FVector4"))
		{
			return MakeVector4Table(State, *static_cast<const FVector4*>(ValuePtr));
		}
		if (IsStructType(Property, "FColor"))
		{
			return MakeColorTable(State, *static_cast<const FColor*>(ValuePtr));
		}
		if (IsStructType(Property, "FQuat"))
		{
			return sol::make_object(State, *static_cast<const FQuat*>(ValuePtr));
		}
		if (IsStructType(Property, "FGuid"))
		{
			return sol::make_object(State, static_cast<const FGuid*>(ValuePtr)->ToString());
		}

		sol::state_view Lua(State);
		sol::table Table = Lua.create_table();
		if (!Property.ScriptStruct)
		{
			return sol::make_object(State, Table);
		}

		TArray<const FProperty*> ChildProperties;
		Property.ScriptStruct->GetAllProperties(ChildProperties);
		for (const FProperty* Child : ChildProperties)
		{
			if (!Child || !Child->Name)
			{
				continue;
			}
			const void* ChildPtr = reinterpret_cast<const uint8*>(ValuePtr) + Child->Offset;
			Table[Child->Name] = FLuaPropertyValueCodec::ReadPropertyToLuaValue(State, *Child, ChildPtr);
		}

		return sol::make_object(State, Table);
	}

	bool WriteLuaVector(sol::object Value, FVector& OutVector)
	{
		if (Value.is<FVector>())
		{
			OutVector = Value.as<FVector>();
			return true;
		}
		if (!Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		return ReadRequiredFloatField(Table, "X", "x", 1, OutVector.X)
			&& ReadRequiredFloatField(Table, "Y", "y", 2, OutVector.Y)
			&& ReadRequiredFloatField(Table, "Z", "z", 3, OutVector.Z);
	}

	bool WriteLuaVector2(sol::object Value, FVector2& OutVector)
	{
		if (Value.is<FVector2>())
		{
			OutVector = Value.as<FVector2>();
			return true;
		}
		if (!Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		return ReadRequiredFloatField(Table, "X", "x", 1, OutVector.X)
			&& ReadRequiredFloatField(Table, "Y", "y", 2, OutVector.Y);
	}

	bool WriteLuaVector4(sol::object Value, FVector4& OutVector)
	{
		if (Value.is<FVector4>())
		{
			OutVector = Value.as<FVector4>();
			return true;
		}
		if (!Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		return ReadRequiredFloatField(Table, "X", "x", 1, OutVector.X)
			&& ReadRequiredFloatField(Table, "Y", "y", 2, OutVector.Y)
			&& ReadRequiredFloatField(Table, "Z", "z", 3, OutVector.Z)
			&& ReadRequiredFloatField(Table, "W", "w", 4, OutVector.W);
	}

	bool WriteLuaColor(sol::object Value, FColor& OutColor)
	{
		if (Value.is<FColor>())
		{
			OutColor = Value.as<FColor>();
			return true;
		}
		if (!Value.is<sol::table>())
		{
			return false;
		}

		sol::table Table = Value.as<sol::table>();
		if (!ReadRequiredFloatField(Table, "R", "r", 1, OutColor.R)
			|| !ReadRequiredFloatField(Table, "G", "g", 2, OutColor.G)
			|| !ReadRequiredFloatField(Table, "B", "b", 3, OutColor.B))
		{
			return false;
		}

		OutColor.A = 1.0f;
		TryReadFloatField(Table, "A", "a", 4, OutColor.A);
		return true;
	}

	bool WriteLuaQuat(sol::object Value, FQuat& OutQuat)
	{
		if (Value.is<FQuat>())
		{
			OutQuat = Value.as<FQuat>();
		}
		else
		{
			if (!Value.is<sol::table>())
			{
				return false;
			}

			sol::table Table = Value.as<sol::table>();
			if (!ReadRequiredFloatField(Table, "X", "x", 1, OutQuat.X)
				|| !ReadRequiredFloatField(Table, "Y", "y", 2, OutQuat.Y)
				|| !ReadRequiredFloatField(Table, "Z", "z", 3, OutQuat.Z)
				|| !ReadRequiredFloatField(Table, "W", "w", 4, OutQuat.W))
			{
				return false;
			}
		}

		if (OutQuat.SizeSquared() <= 1.e-8f)
		{
			return false;
		}
		OutQuat.Normalize();
		return true;
	}

	bool WriteLuaGuid(sol::object Value, FGuid& OutGuid)
	{
		return Value.is<FString>() && FGuid::Parse(Value.as<FString>(), OutGuid);
	}

	sol::object MakeVector2Table(sol::this_state State, const FVector2& Value)
	{
		sol::state_view Lua(State);
		sol::table Table = Lua.create_table();
		Table["X"] = Value.X;
		Table["Y"] = Value.Y;
		return sol::make_object(State, Table);
	}

	sol::object MakeVector4Table(sol::this_state State, const FVector4& Value)
	{
		sol::state_view Lua(State);
		sol::table Table = Lua.create_table();
		Table["X"] = Value.X;
		Table["Y"] = Value.Y;
		Table["Z"] = Value.Z;
		Table["W"] = Value.W;
		return sol::make_object(State, Table);
	}

	sol::object MakeColorTable(sol::this_state State, const FColor& Value)
	{
		sol::state_view Lua(State);
		sol::table Table = Lua.create_table();
		Table["R"] = Value.R;
		Table["G"] = Value.G;
		Table["B"] = Value.B;
		Table["A"] = Value.A;
		return sol::make_object(State, Table);
	}
}
