#include "JsonReader.h"

FArchive& FJsonReader::operator<<(bool& Value)
{
	json::JSON& Current = *ScopeStack.back();

	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = Current[CurrentIdx++].ToBool();
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = Current[CurrentKey.c_str()].ToBool();
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(uint32& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = static_cast<uint32>(Current[CurrentIdx++].ToInt());
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = static_cast<uint32>(Current[CurrentKey.c_str()].ToInt());
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonReader::operator<<(int32& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = Current[CurrentIdx++].ToInt();
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = Current[CurrentKey.c_str()].ToInt();
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonReader::operator<<(float& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = static_cast<float>(Current[CurrentIdx++].ToFloat());
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = static_cast<float>(Current[CurrentKey.c_str()].ToFloat());
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonReader::operator<<(const char* Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = Current[CurrentIdx++].ToString().c_str();
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = Current[CurrentKey.c_str()].ToString().c_str();
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value;
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FString& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = Current[CurrentIdx++].ToString();
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = Current[CurrentKey.c_str()].ToString();
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value;
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FName& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			Value = FName(Current[CurrentIdx++].ToString().c_str());
		}
	}
	else if (!CurrentKey.empty())
	{
		Value = FName(Current[CurrentKey.c_str()].ToString().c_str());
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value.ToString();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FVector2& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			auto& VecArr = Current[CurrentIdx++];
			if (VecArr.length() >= 2)
			{
				Value.X = static_cast<float>(VecArr[0].ToFloat());
				Value.Y = static_cast<float>(VecArr[1].ToFloat());
			}
		}
	}
	else if (!CurrentKey.empty())
	{
		auto& VecArr = Current[CurrentKey.c_str()];
		if (VecArr.length() >= 2)
		{
			Value.X = static_cast<float>(VecArr[0].ToFloat());
			Value.Y = static_cast<float>(VecArr[1].ToFloat());
		}
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FVector& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			auto& VecArr = Current[CurrentIdx++];
			if (VecArr.length() >= 3)
			{
				Value.X = static_cast<float>(VecArr[0].ToFloat());
				Value.Y = static_cast<float>(VecArr[1].ToFloat());
				Value.Z = static_cast<float>(VecArr[2].ToFloat());
			}
		}
	}
	else if (!CurrentKey.empty())
	{
		auto& VecArr = Current[CurrentKey.c_str()];
		if (VecArr.length() >= 3)
		{
			Value.X = static_cast<float>(VecArr[0].ToFloat());
			Value.Y = static_cast<float>(VecArr[1].ToFloat());
			Value.Z = static_cast<float>(VecArr[2].ToFloat());
		}
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FVector4& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			auto& VecArr = Current[CurrentIdx++];
			if (VecArr.length() >= 4)
			{
				Value.X = static_cast<float>(VecArr[0].ToFloat());
				Value.Y = static_cast<float>(VecArr[1].ToFloat());
				Value.Z = static_cast<float>(VecArr[2].ToFloat());
				Value.W = static_cast<float>(VecArr[3].ToFloat());
			}
		}
	}
	else if (!CurrentKey.empty())
	{
		auto& VecArr = Current[CurrentKey.c_str()];
		if (VecArr.length() >= 4)
		{
			Value.X = static_cast<float>(VecArr[0].ToFloat());
			Value.Y = static_cast<float>(VecArr[1].ToFloat());
			Value.Z = static_cast<float>(VecArr[2].ToFloat());
			Value.W = static_cast<float>(VecArr[3].ToFloat());
		}
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FColor& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			auto& ColorArr = Current[CurrentIdx++];
			if (ColorArr.length() >= 3)
			{
				Value.R = static_cast<float>(ColorArr[0].ToFloat());
				Value.G = static_cast<float>(ColorArr[1].ToFloat());
				Value.B = static_cast<float>(ColorArr[2].ToFloat());
				Value.A = (ColorArr.length() >= 4) ? static_cast<float>(ColorArr[3].ToFloat()) : 1.0f;
			}
		}
	}
	else if (!CurrentKey.empty())
	{
		auto& ColorArr = Current[CurrentKey.c_str()];
		if (ColorArr.length() >= 3)
		{
			Value.R = static_cast<float>(ColorArr[0].ToFloat());
			Value.G = static_cast<float>(ColorArr[1].ToFloat());
			Value.B = static_cast<float>(ColorArr[2].ToFloat());
			Value.A = (ColorArr.length() >= 4) ? static_cast<float>(ColorArr[3].ToFloat()) : 1.0f;
		}
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonReader::operator<<(FMatrix& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		int32& CurrentIdx = ArrayIndexStack.back();
		if (CurrentIdx < (int32)Current.length())
		{
			auto& MatArr = Current[CurrentIdx++];
			if (MatArr.length() >= 16)
			{
				for (int i = 0; i < 16; ++i)
				{
					Value.M[i / 4][i % 4] = static_cast<float>(MatArr[i].ToFloat());
				}
			}
		}
	}
	else if (!CurrentKey.empty())
	{
		auto& MatArr = Current[CurrentKey.c_str()];
		if (MatArr.length() >= 16)
		{
			for (int i = 0; i < 16; ++i)
			{
				Value.M[i / 4][i % 4] = static_cast<float>(MatArr[i].ToFloat());
			}
		}
		CurrentKey.clear();
	}
	return *this;
}
