#pragma once

#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"
#include <cstring>

class FJsonArchive : public FArchive
{
public:
	FJsonArchive(json::JSON& InRoot, bool bInIsSaving)
	{
		bIsSaving = bInIsSaving;
		bIsLoading = !bInIsSaving;
		Stack.push_back(&InRoot);
	}

	void Serialize(void* Data, size_t Num) override
	{
		if (!Data || Num == 0)
		{
			return;
		}

		if (bIsSaving)
		{
			json::JSON Arr = json::Array();
			const uint8* Bytes = static_cast<const uint8*>(Data);
			for (size_t Index = 0; Index < Num; ++Index)
			{
				Arr.append(static_cast<int>(Bytes[Index]));
			}
			*Current() = Arr;
			return;
		}

		json::JSON* Node = Current();
		if (!Node || Node->JSONType() != json::JSON::Class::Array)
		{
			std::memset(Data, 0, Num);
			return;
		}

		uint8* Bytes = static_cast<uint8*>(Data);
		for (size_t Index = 0; Index < Num; ++Index)
		{
			Bytes[Index] = static_cast<uint8>((*Node)[static_cast<unsigned>(Index)].ToInt());
		}
	}

	bool UsesCustomObjectReferenceSerialization() const override { return true; }

	bool HasProperty(const char* Name) const override
	{
		const json::JSON* Node = Current();
		return bIsSaving || (Node && Name && Node->hasKey(Name));
	}

	void BeginObject() override
	{
		if (bIsSaving)
		{
			*Current() = json::Object();
		}
	}

	void BeginProperty(const char* Name) override
	{
		json::JSON* Node = Current();
		if (!Node || !Name)
		{
			Stack.push_back(nullptr);
			return;
		}

		Stack.push_back(&(*Node)[Name]);
	}

	void EndProperty() override
	{
		if (Stack.size() > 1)
		{
			Stack.pop_back();
		}
	}

	bool BeginArray(uint32& Num) override
	{
		json::JSON* Node = Current();
		if (!Node)
		{
			Num = 0;
			return true;
		}

		if (bIsSaving)
		{
			*Node = json::Array();
		}
		else
		{
			Num = Node->JSONType() == json::JSON::Class::Array ? static_cast<uint32>(Node->size()) : 0;
		}
		return true;
	}

	void BeginArrayElement(uint32 Index) override
	{
		json::JSON* Node = Current();
		Stack.push_back(Node ? &(*Node)[static_cast<unsigned>(Index)] : nullptr);
	}

	void EndArrayElement() override
	{
		if (Stack.size() > 1)
		{
			Stack.pop_back();
		}
	}

	void SerializeBool(bool& Value) override
	{
		if (bIsSaving) *Current() = Value;
		else Value = Current() ? Current()->ToBool() : false;
	}

	void SerializeInt32(int32& Value) override
	{
		if (bIsSaving) *Current() = Value;
		else Value = Current() ? static_cast<int32>(Current()->ToInt()) : 0;
	}

	void SerializeUInt32(uint32& Value) override
	{
		if (bIsSaving) *Current() = static_cast<int>(Value);
		else Value = Current() ? static_cast<uint32>(Current()->ToInt()) : 0;
	}

	void SerializeFloat(float& Value) override
	{
		if (bIsSaving) *Current() = static_cast<double>(Value);
		else Value = Current() ? static_cast<float>(Current()->ToFloat()) : 0.0f;
	}

	void SerializeString(FString& Str) override
	{
		if (bIsSaving) *Current() = Str;
		else Str = Current() ? Current()->ToString() : FString();
	}

	void SerializeName(FName& Name) override
	{
		FString Str = bIsSaving ? Name.ToString() : FString();
		SerializeString(Str);
		if (bIsLoading)
		{
			Name = FName(Str);
		}
	}

	void SerializeVector(FVector& Value) override
	{
		SerializeFloatArray(Value.Data, 3);
	}

	void SerializeVector4(FVector4& Value) override
	{
		SerializeFloatArray(Value.Data, 4);
	}

	void SerializeRotator(FRotator& Value) override
	{
		json::JSON* Node = Current();
		if (!Node)
		{
			return;
		}

		if (bIsSaving)
		{
			*Node = json::Array();
			(*Node)[0] = static_cast<double>(Value.Pitch);
			(*Node)[1] = static_cast<double>(Value.Yaw);
			(*Node)[2] = static_cast<double>(Value.Roll);
			return;
		}

		Value.Pitch = static_cast<float>((*Node)[0].ToFloat());
		Value.Yaw = static_cast<float>((*Node)[1].ToFloat());
		Value.Roll = static_cast<float>((*Node)[2].ToFloat());
	}

	void SerializeObjectReference(UObject*& Object) override
	{
		uint32 ObjectId = 0;
		if (bIsSaving)
		{
			ObjectId = Object ? ResolveJsonObjectId(Object) : 0;
		}

		json::JSON* Node = Current();
		if (bIsSaving)
		{
			if (ObjectId == 0)
			{
				*Node = json::JSON();
			}
			else
			{
				*Node = json::Object();
				(*Node)["ObjectId"] = static_cast<int>(ObjectId);
			}
			return;
		}

		if (!Node || Node->IsNull())
		{
			Object = nullptr;
			return;
		}

		if (Node->JSONType() == json::JSON::Class::Object && Node->hasKey("ObjectId"))
		{
			ObjectId = static_cast<uint32>((*Node)["ObjectId"].ToInt());
		}
		Object = ResolveJsonObjectReference(ObjectId);
	}

protected:
	virtual uint32 ResolveJsonObjectId(const UObject* /*Object*/) const { return 0; }
	virtual UObject* ResolveJsonObjectReference(uint32 /*ObjectId*/) const { return nullptr; }

private:
	json::JSON* Current()
	{
		return Stack.empty() ? nullptr : Stack.back();
	}

	const json::JSON* Current() const
	{
		return Stack.empty() ? nullptr : Stack.back();
	}

	void SerializeFloatArray(float* Values, uint32 Num)
	{
		json::JSON* Node = Current();
		if (!Node)
		{
			return;
		}

		if (bIsSaving)
		{
			*Node = json::Array();
			for (uint32 Index = 0; Index < Num; ++Index)
			{
				(*Node)[static_cast<unsigned>(Index)] = static_cast<double>(Values[Index]);
			}
			return;
		}

		for (uint32 Index = 0; Index < Num; ++Index)
		{
			Values[Index] = static_cast<float>((*Node)[static_cast<unsigned>(Index)].ToFloat());
		}
	}

	TArray<json::JSON*> Stack;
};
