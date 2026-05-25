#include "Serialization/ObjectGraphSerializer.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Object/Property.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	constexpr const char* ObjectGraphFormatName = "JSEngine.ObjectGraph";
	constexpr int32 ObjectGraphFormatVersion = 1;

	FString GetJsonString(json::JSON& Object, const char* Key, const FString& DefaultValue = "");
	uint32 GetJsonUInt(json::JSON& Object, const char* Key, uint32 DefaultValue = 0);
	int32 GetJsonInt(json::JSON& Object, const char* Key, int32 DefaultValue = 0);
	bool IsObjectAssignableTo(UObject* Object, UClass* ExpectedClass);
	void CollectObjectReferences(UObject* Object, TArray<UObject*>& OutReferences);
}

uint32 FObjectGraphReferenceResolver::GetObjectId(UObject* Object) const
{
	if (!Object)
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	return It != ObjectToId.end() ? It->second : 0;
}

UObject* FObjectGraphReferenceResolver::ResolveObjectId(uint32 Id, UClass* ExpectedClass) const
{
	if (Id == 0)
	{
		return nullptr;
	}

	auto It = IdToObject.find(Id);
	if (It == IdToObject.end())
	{
		return nullptr;
	}

	UObject* Object = It->second;
	return IsObjectAssignableTo(Object, ExpectedClass) ? Object : nullptr;
}

void FObjectGraphReferenceResolver::AddObject(uint32 Id, UObject* Object)
{
	if (Id == 0 || !Object)
	{
		return;
	}

	IdToObject[Id] = Object;
	ObjectToId[Object] = Id;
}

void FObjectGraphReferenceResolver::AddObjectId(UObject* Object, uint32 Id)
{
	AddObject(Id, Object);
}

void FObjectGraphReferenceResolver::Clear()
{
	IdToObject.clear();
	ObjectToId.clear();
}

bool FObjectGraphSerializer::SaveToFile(const FString& Path, UObject* RootObject, const FString& AssetType)
{
	Clear();

	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || !RootObject)
	{
		return false;
	}

	CollectGraph(RootObject);

	const uint32 RootObjectId = Resolver.GetObjectId(RootObject);
	if (RootObjectId == 0)
	{
		return false;
	}

	json::JSON Root = json::Object();
	Root["Format"] = ObjectGraphFormatName;
	Root["AssetType"] = AssetType;
	Root["Version"] = ObjectGraphFormatVersion;
	Root["RootObjectId"] = static_cast<int32>(RootObjectId);
	Root["Objects"] = json::Array();

	json::JSON& ObjectArray = Root["Objects"];
	for (UObject* Object : Objects)
	{
		if (!Object)
		{
			continue;
		}

		json::JSON ObjectJson = json::Object();
		ObjectJson["Id"] = static_cast<int32>(Resolver.GetObjectId(Object));
		ObjectJson["Type"] = Object->GetClassName();
		ObjectJson["ObjectName"] = Object->GetFName().ToString();
		ObjectJson["Properties"] = json::Object();

		FJsonWriter Writer(ObjectJson["Properties"]);
		Writer.SetObjectResolver(&Resolver);
		Object->SerializeProperties(Writer);

		ObjectArray.append(ObjectJson);
	}

	const std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
	std::error_code ErrorCode;
	std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
	if (ErrorCode)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Failed to create directory: %s", NormalizedPath.c_str());
		return false;
	}

	std::ofstream OutFile(FilePath);
	if (!OutFile.is_open())
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Failed to open file for writing: %s", NormalizedPath.c_str());
		return false;
	}

	OutFile << Root.dump(4);
	return true;
}

UObject* FObjectGraphSerializer::LoadFromFile(const FString& Path, const FString& ExpectedRootType)
{
	Clear();

	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	std::ifstream InFile(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
	if (!InFile.is_open())
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Failed to open file: %s", NormalizedPath.c_str());
		return nullptr;
	}

	const FString FileContent((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
	json::JSON Root = json::JSON::Load(FileContent);
	if (Root.JSONType() != json::JSON::Class::Object)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Invalid json object graph: %s", NormalizedPath.c_str());
		return nullptr;
	}

	if (GetJsonString(Root, "Format") != ObjectGraphFormatName)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Unsupported object graph format: %s", NormalizedPath.c_str());
		return nullptr;
	}

	if (GetJsonInt(Root, "Version") != ObjectGraphFormatVersion)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Unsupported object graph version: %s", NormalizedPath.c_str());
		return nullptr;
	}

	if (!ExpectedRootType.empty() && GetJsonString(Root, "AssetType") != ExpectedRootType)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Unexpected asset type: %s", NormalizedPath.c_str());
		return nullptr;
	}

	if (!Root.hasKey("Objects") || Root["Objects"].JSONType() != json::JSON::Class::Array)
	{
		UE_LOG_ERROR("[ObjectGraphSerializer] Missing object table: %s", NormalizedPath.c_str());
		return nullptr;
	}

	json::JSON& ObjectArray = Root["Objects"];
	Objects.reserve(ObjectArray.length());

	for (int32 Index = 0; Index < static_cast<int32>(ObjectArray.length()); ++Index)
	{
		json::JSON& ObjectJson = ObjectArray.at(Index);
		if (ObjectJson.JSONType() != json::JSON::Class::Object)
		{
			DestroyCreatedObjects(nullptr, false);
			return nullptr;
		}

		const uint32 ObjectId = GetJsonUInt(ObjectJson, "Id");
		const FString TypeName = GetJsonString(ObjectJson, "Type");
		const FString ObjectName = GetJsonString(ObjectJson, "ObjectName");
		UClass* Class = FReflectionRegistry::Get().FindClass(TypeName);
		UObject* Object = NewObject(Class);
		if (ObjectId == 0 || !Object)
		{
			UObjectManager::Get().DestroyObject(Object);
			DestroyCreatedObjects(nullptr, false);
			UE_LOG_ERROR("[ObjectGraphSerializer] Failed to create object: %s", TypeName.c_str());
			return nullptr;
		}

		if (!ObjectName.empty())
		{
			Object->SetFName(FName(ObjectName));
		}

		Objects.push_back(Object);
		Resolver.AddObject(ObjectId, Object);
	}

	for (int32 Index = 0; Index < static_cast<int32>(ObjectArray.length()); ++Index)
	{
		json::JSON& ObjectJson = ObjectArray.at(Index);
		const uint32 ObjectId = GetJsonUInt(ObjectJson, "Id");
		UObject* Object = Resolver.ResolveObjectId(ObjectId, nullptr);
		if (!Object)
		{
			DestroyCreatedObjects(nullptr, false);
			return nullptr;
		}

		if (!ObjectJson.hasKey("Properties") || ObjectJson["Properties"].JSONType() != json::JSON::Class::Object)
		{
			continue;
		}

		FJsonReader Reader(ObjectJson["Properties"]);
		Reader.SetObjectResolver(&Resolver);
		Object->SerializeProperties(Reader);
	}

	const uint32 RootObjectId = GetJsonUInt(Root, "RootObjectId");
	UObject* UntypedRootObject = Resolver.ResolveObjectId(RootObjectId, nullptr);
	UObject* RootObject = Resolver.ResolveObjectId(
		RootObjectId,
		ExpectedRootType.empty() ? nullptr : FReflectionRegistry::Get().FindClass(ExpectedRootType));
	if (!RootObject)
	{
		DestroyCreatedObjects(UntypedRootObject, UntypedRootObject != nullptr);
		UE_LOG_ERROR("[ObjectGraphSerializer] Failed to resolve root object: %s", NormalizedPath.c_str());
		return nullptr;
	}

	return RootObject;
}

void FObjectGraphSerializer::Clear()
{
	Objects.clear();
	ObjectToId.clear();
	Resolver.Clear();
}

void FObjectGraphSerializer::CollectGraph(UObject* RootObject)
{
	if (!RootObject)
	{
		return;
	}

	GetOrAssignObjectId(RootObject);

	for (size_t Index = 0; Index < Objects.size(); ++Index)
	{
		UObject* Object = Objects[Index];
		TArray<UObject*> References;
		CollectObjectReferences(Object, References);
		for (UObject* ReferencedObject : References)
		{
			GetOrAssignObjectId(ReferencedObject);
		}
	}
}

uint32 FObjectGraphSerializer::GetOrAssignObjectId(UObject* Object)
{
	if (!Object)
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	if (It != ObjectToId.end())
	{
		return It->second;
	}

	const uint32 NewId = static_cast<uint32>(Objects.size()) + 1;
	Objects.push_back(Object);
	ObjectToId[Object] = NewId;
	Resolver.AddObject(NewId, Object);
	return NewId;
}

void FObjectGraphSerializer::DestroyCreatedObjects(UObject* RootObject, bool bPropertiesLinked)
{
	if (bPropertiesLinked && !RootObject && !Objects.empty())
	{
		RootObject = Objects.front();
	}

	if (bPropertiesLinked && RootObject)
	{
		UObjectManager::Get().DestroyObject(RootObject);
		Clear();
		return;
	}

	for (UObject* Object : Objects)
	{
		UObjectManager::Get().DestroyObject(Object);
	}

	Clear();
}

namespace
{
	FString GetJsonString(json::JSON& Object, const char* Key, const FString& DefaultValue)
	{
		return Object.hasKey(Key) ? Object[Key].ToString() : DefaultValue;
	}

	uint32 GetJsonUInt(json::JSON& Object, const char* Key, uint32 DefaultValue)
	{
		return Object.hasKey(Key) ? static_cast<uint32>(Object[Key].ToInt()) : DefaultValue;
	}

	int32 GetJsonInt(json::JSON& Object, const char* Key, int32 DefaultValue)
	{
		return Object.hasKey(Key) ? Object[Key].ToInt() : DefaultValue;
	}

	bool IsObjectAssignableTo(UObject* Object, UClass* ExpectedClass)
	{
		return Object && (!ExpectedClass || Object->IsA(ExpectedClass));
	}

	void CollectObjectReferences(UObject* Object, TArray<UObject*>& OutReferences)
	{
		if (!Object || !Object->GetClass())
		{
			return;
		}

		TArray<const FProperty*> Properties;
		Object->GetClass()->GetAllProperties(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!Property || Property->IsTransient())
			{
				continue;
			}

			FReferenceCollector Collector;
			Property->VisitReferences(Collector, Property->GetValuePtr(Object));
			for (UObject* ReferencedObject : Collector.GetReferencedObjects())
			{
				if (ReferencedObject && std::find(OutReferences.begin(), OutReferences.end(), ReferencedObject) == OutReferences.end())
				{
					OutReferences.push_back(ReferencedObject);
				}
			}
		}
	}
}
