#include "JsonWriter.h"

FArchive& FJsonWriter::operator<<(bool& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(uint32& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(int32& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(float& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = std::to_string(Value);
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(const char* Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value;
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FString& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value);
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value;
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value;
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FName& Value)
{
	json::JSON& Current = *ScopeStack.back();
	if (Current.JSONType() == json::JSON::Class::Array)
	{
		Current.append(Value.ToString());
	}
	else if (!CurrentKey.empty())
	{
		(*ScopeStack.back())[CurrentKey] = Value.ToString();
		CurrentKey.clear();
	}
	else
	{
		CurrentKey = Value.ToString();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FVector2& Value)
{
	if (ScopeStack.back()->JSONType() == json::JSON::Class::Array)
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y);
		ScopeStack.back()->append(VecArray);
	}
	else if (!CurrentKey.empty())
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y);
		(*ScopeStack.back())[CurrentKey] = VecArray;
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FVector& Value)
{
	if (ScopeStack.back()->JSONType() == json::JSON::Class::Array)
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y, Value.Z);
		ScopeStack.back()->append(VecArray);
	}
	else if (!CurrentKey.empty())
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y, Value.Z);
		(*ScopeStack.back())[CurrentKey] = VecArray;
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FVector4& Value)
{
	if (ScopeStack.back()->JSONType() == json::JSON::Class::Array)
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y, Value.Z, Value.W);
		ScopeStack.back()->append(VecArray);
	}
	else if (!CurrentKey.empty())
	{
		json::JSON VecArray = json::Array(Value.X, Value.Y, Value.Z, Value.W);
		(*ScopeStack.back())[CurrentKey] = VecArray;
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FColor& Value)
{
	if (ScopeStack.back()->JSONType() == json::JSON::Class::Array)
	{
		json::JSON ColorArray = json::Array(Value.R, Value.G, Value.B, Value.A);
		ScopeStack.back()->append(ColorArray);
	}
	else if (!CurrentKey.empty())
	{
		json::JSON ColorArray = json::Array(Value.R, Value.G, Value.B, Value.A);
		(*ScopeStack.back())[CurrentKey] = ColorArray;
		CurrentKey.clear();
	}
	return *this;
}

FArchive& FJsonWriter::operator<<(FMatrix& Value)
{
	if (ScopeStack.back()->JSONType() == json::JSON::Class::Array)
	{
		json::JSON MatrixArray = json::Array();
		for (int i = 0; i < 4; ++i)
		{
			json::JSON RowArray = json::Array(Value.M[i][0], Value.M[i][1], Value.M[i][2], Value.M[i][3]);
			MatrixArray.append(RowArray);
		}
		ScopeStack.back()->append(MatrixArray);
	}
	else if (!CurrentKey.empty())
	{
		json::JSON MatrixArray = json::Array();
		for (int i = 0; i < 4; ++i)
		{
			json::JSON RowArray = json::Array(Value.M[i][0], Value.M[i][1], Value.M[i][2], Value.M[i][3]);
			MatrixArray.append(RowArray);
		}
		(*ScopeStack.back())[CurrentKey] = MatrixArray;
		CurrentKey.clear();
	}
	return *this;
}
