#include "Core/Types/PropertyTypes.h"

#include "Object/Object.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
	FString TrimString(const FString& Value)
	{
		size_t Begin = 0;
		while (Begin < Value.size() && std::isspace(static_cast<unsigned char>(Value[Begin])))
		{
			++Begin;
		}

		size_t End = Value.size();
		while (End > Begin && std::isspace(static_cast<unsigned char>(Value[End - 1])))
		{
			--End;
		}

		return Value.substr(Begin, End - Begin);
	}

	FString StripQuotes(const FString& Value)
	{
		FString Trimmed = TrimString(Value);
		if (Trimmed.size() >= 2 &&
			((Trimmed.front() == '"' && Trimmed.back() == '"') ||
			 (Trimmed.front() == '\'' && Trimmed.back() == '\'')))
		{
			return Trimmed.substr(1, Trimmed.size() - 2);
		}
		return Trimmed;
	}

	FString StripEnumQualifier(const FString& Value)
	{
		FString Trimmed = StripQuotes(Value);
		const size_t ScopePos = Trimmed.rfind("::");
		return ScopePos != FString::npos ? Trimmed.substr(ScopePos + 2) : Trimmed;
	}

	const FString* FindMetadataValue(const FProperty& Property, const FString& Key)
	{
		auto It = Property.Metadata.find(Key);
		return It != Property.Metadata.end() ? &It->second : nullptr;
	}

	const FProperty* FindPropertyByName(UStruct* StructType, const FString& Name)
	{
		if (!StructType)
		{
			return nullptr;
		}

		TArray<const FProperty*> Properties;
		StructType->GetPropertyRefs(Properties);
		for (const FProperty* Property : Properties)
		{
			if (Property && Property->Name && Name == Property->Name)
			{
				return Property;
			}
		}
		return nullptr;
	}

	bool ReadIntegerLikeProperty(const FProperty& Property, void* ValuePtr, int64& OutValue)
	{
		if (!ValuePtr)
		{
			return false;
		}

		switch (Property.GetType())
		{
		case EPropertyType::Bool:
			OutValue = *static_cast<bool*>(ValuePtr) ? 1 : 0;
			return true;
		case EPropertyType::ByteBool:
			OutValue = *static_cast<uint8*>(ValuePtr);
			return true;
		case EPropertyType::Int:
			OutValue = *static_cast<int32*>(ValuePtr);
			return true;
		case EPropertyType::Enum:
		{
			const uint32 EnumSize = Property.GetEnumType() ? Property.GetEnumType()->GetSize() : sizeof(int32);
			uint64 RawValue = 0;
			std::memcpy(&RawValue, ValuePtr, (std::min)(static_cast<size_t>(EnumSize), sizeof(RawValue)));
			OutValue = static_cast<int64>(RawValue);
			return true;
		}
		default:
			return false;
		}
	}

	bool PropertyValueMatchesLiteral(const FProperty& Property, void* ValuePtr, const FString& Literal)
	{
		const FString Expected = StripEnumQualifier(Literal);

		if (Property.GetType() == EPropertyType::Enum)
		{
			int64 EnumIndex = 0;
			if (!ReadIntegerLikeProperty(Property, ValuePtr, EnumIndex))
			{
				return false;
			}

			if (const FEnum* EnumType = Property.GetEnumType())
			{
				if (EnumIndex >= 0 && static_cast<uint32>(EnumIndex) < EnumType->GetCount())
				{
					const char** Names = EnumType->GetNames();
					if (Names && Names[EnumIndex] && Expected == Names[EnumIndex])
					{
						return true;
					}
				}
			}
		}

		int64 IntegerValue = 0;
		if (ReadIntegerLikeProperty(Property, ValuePtr, IntegerValue))
		{
			char* EndPtr = nullptr;
			const long long ExpectedValue = std::strtoll(Expected.c_str(), &EndPtr, 10);
			if (EndPtr && *EndPtr == '\0')
			{
				return IntegerValue == ExpectedValue;
			}

			if (Expected == "true")
			{
				return IntegerValue != 0;
			}
			if (Expected == "false")
			{
				return IntegerValue == 0;
			}
		}

		return false;
	}

	bool EvaluateEditCondition(const FProperty& Property, UStruct* StructType, void* ContainerPtr)
	{
		const FString* ConditionValue = FindMetadataValue(Property, "editcondition");
		if (!ConditionValue || ConditionValue->empty())
		{
			return true;
		}

		FString Condition = TrimString(*ConditionValue);
		bool bInvertImplicitBool = false;
		if (!Condition.empty() && Condition.front() == '!')
		{
			bInvertImplicitBool = true;
			Condition = TrimString(Condition.substr(1));
		}

		FString Operator;
		size_t OperatorPos = Condition.find("==");
		if (OperatorPos != FString::npos)
		{
			Operator = "==";
		}
		else
		{
			OperatorPos = Condition.find("!=");
			if (OperatorPos != FString::npos)
			{
				Operator = "!=";
			}
			else
			{
				OperatorPos = Condition.find('=');
				if (OperatorPos != FString::npos)
				{
					Operator = "=";
				}
			}
		}

		FString SourceName = Condition;
		FString Literal;
		if (!Operator.empty())
		{
			SourceName = TrimString(Condition.substr(0, OperatorPos));
			Literal = TrimString(Condition.substr(OperatorPos + Operator.size()));
		}

		const FProperty* SourceProperty = FindPropertyByName(StructType, SourceName);
		if (!SourceProperty)
		{
			return true;
		}

		void* SourcePtr = SourceProperty->GetValuePtrFor(ContainerPtr);
		if (!SourcePtr)
		{
			return true;
		}

		if (Operator.empty())
		{
			int64 IntegerValue = 0;
			const bool bValue = ReadIntegerLikeProperty(*SourceProperty, SourcePtr, IntegerValue) && IntegerValue != 0;
			return bInvertImplicitBool ? !bValue : bValue;
		}

		const bool bMatches = PropertyValueMatchesLiteral(*SourceProperty, SourcePtr, Literal);
		return Operator == "!=" ? !bMatches : bMatches;
	}
}

const char* FPropertyValue::GetName() const
{
	return Property && Property->Name ? Property->Name : "";
}

const char* FPropertyValue::GetDisplayName() const
{
	return Property && Property->DisplayName ? Property->DisplayName : GetName();
}

const char* FPropertyValue::GetCategory() const
{
	return Property && Property->Category ? Property->Category : "";
}

EPropertyType FPropertyValue::GetType() const
{
	return Property ? Property->GetType() : EPropertyType::Bool;
}

float FPropertyValue::GetMin() const
{
	return Property ? Property->GetMin() : 0.0f;
}

float FPropertyValue::GetMax() const
{
	return Property ? Property->GetMax() : 0.0f;
}

float FPropertyValue::GetSpeed() const
{
	return Property ? Property->GetSpeed() : 0.1f;
}

UStruct* FPropertyValue::GetStructType() const
{
	return Property ? Property->GetStructType() : nullptr;
}

const FEnum* FPropertyValue::GetEnumType() const
{
	return Property ? Property->GetEnumType() : nullptr;
}

const TMap<FString, FString>& FPropertyValue::GetMetadata() const
{
	static const TMap<FString, FString> EmptyMetadata;
	return Property ? Property->Metadata : EmptyMetadata;
}

void* FPropertyValue::GetValuePtr() const
{
	return Property ? Property->GetValuePtrFor(ContainerPtr) : nullptr;
}

bool FPropertyValue::PassesEditCondition() const
{
	if (!Property)
	{
		return true;
	}

	UStruct* StructType = nullptr;
	if (Object && ContainerPtr == Object)
	{
		StructType = Object->GetClass();
	}

	return EvaluateEditCondition(*Property, StructType, ContainerPtr);
}

void FPropertyValue::GetStructChildren(TArray<FPropertyValue>& OutProps) const
{
	OutProps.clear();
	UStruct* StructType = GetStructType();
	void* ValuePtr = GetValuePtr();
	if (!StructType || !ValuePtr)
	{
		return;
	}

	TArray<const FProperty*> ChildProperties;
	StructType->GetPropertyRefs(ChildProperties);
	for (const FProperty* ChildProperty : ChildProperties)
	{
		if (!ChildProperty || (ChildProperty->Flags & PF_Edit) == 0 || !ChildProperty->GetValuePtrFor(ValuePtr))
		{
			continue;
		}
		if (!EvaluateEditCondition(*ChildProperty, StructType, ValuePtr))
		{
			continue;
		}

		OutProps.push_back(ChildProperty->ToValue(ValuePtr, Object));
	}
}

void FProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	FPropertySerializeContext Context;
	Context.Owner = Object;
	SerializeValue(GetValuePtrFor(Object), Ar, Context);
}

void FProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
{
	(void)Context;
	SerializeValue(ValuePtr, Ar);
}

void FProperty::Serialize(void* Container, FArchive& Ar) const
{
	SerializeValue(GetValuePtrFor(Container), Ar);
}
