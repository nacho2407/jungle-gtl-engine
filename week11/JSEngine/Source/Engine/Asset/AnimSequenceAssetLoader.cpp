#include "Asset/AnimSequenceAssetLoader.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
    constexpr int32 MaxAnimNotifyActionType = static_cast<int32>(EAnimNotifyActionType::PlayEffect);

    FString NormalizeAnimSequenceAssetPath(const FString& Path)
    {
        return FPaths::Normalize(Path);
    }

    bool IsAnimSequenceAssetPath(const FString& Path)
    {
        FString LowerPath = FPaths::Normalize(Path);
        std::transform(
            LowerPath.begin(),
            LowerPath.end(),
            LowerPath.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });

        return std::filesystem::path(FPaths::ToWide(LowerPath)).extension() == L".animsequence";
    }

    bool IsJsonObject(const json::JSON& Node)
    {
        return Node.JSONType() == json::JSON::Class::Object;
    }

    bool IsJsonArray(const json::JSON& Node)
    {
        return Node.JSONType() == json::JSON::Class::Array;
    }

    bool ReadFloatValue(json::JSON& Node, float& OutValue)
    {
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

    bool ReadIntValue(json::JSON& Node, int32& OutValue)
    {
        if (Node.JSONType() != json::JSON::Class::Integral)
        {
            return false;
        }

        OutValue = static_cast<int32>(Node.ToInt());
        return true;
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

    bool ReadNotifyValue(json::JSON& Node, FAnimNotifyEvent& OutNotify)
    {
        if (!IsJsonObject(Node))
        {
            return false;
        }

        FString NotifyName;
        FString EventId;
        int32 ActionType = 0;

        if (!Node.hasKey("TriggerTime") || !ReadFloatValue(Node["TriggerTime"], OutNotify.TriggerTime) ||
            !Node.hasKey("Duration") || !ReadFloatValue(Node["Duration"], OutNotify.Duration) ||
            !Node.hasKey("NotifyName") || !ReadStringValue(Node["NotifyName"], NotifyName) ||
            !Node.hasKey("ActionType") || !ReadIntValue(Node["ActionType"], ActionType) ||
            !Node.hasKey("EventId") || !ReadStringValue(Node["EventId"], EventId))
        {
            return false;
        }

        if (Node.hasKey("Payload") && !ReadStringValue(Node["Payload"], OutNotify.Payload))
        {
            return false;
        }

        OutNotify.NotifyName = FName(NotifyName);
        OutNotify.ActionType = static_cast<EAnimNotifyActionType>(std::clamp(ActionType, 0, MaxAnimNotifyActionType));
        OutNotify.EventId = FName(EventId);
        return true;
    }

    bool ReadNotifies(json::JSON& Root, const FString& Path, TArray<FAnimNotifyEvent>& OutNotifies, bool& bOutHasAuthoredNotifies)
    {
        bOutHasAuthoredNotifies = false;
        OutNotifies.clear();

        if (!Root.hasKey("Notifies"))
        {
            return true;
        }

        bOutHasAuthoredNotifies = true;
        json::JSON& NotifiesNode = Root["Notifies"];
        if (!IsJsonArray(NotifiesNode))
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Notifies must be an array: %s", Path.c_str());
            return false;
        }

        OutNotifies.reserve(NotifiesNode.length() > 0 ? static_cast<size_t>(NotifiesNode.length()) : 0);
        for (int32 NotifyIndex = 0; NotifyIndex < NotifiesNode.length(); ++NotifyIndex)
        {
            FAnimNotifyEvent Notify;
            if (!ReadNotifyValue(NotifiesNode[static_cast<unsigned>(NotifyIndex)], Notify))
            {
                UE_LOG_ERROR(
                    "[AnimSequenceAssetLoader] Invalid notify entry. Path=%s NotifyIndex=%d",
                    Path.c_str(),
                    NotifyIndex);
                return false;
            }

            OutNotifies.push_back(Notify);
        }

        return true;
    }

    void WriteNotifies(json::JSON& Root, const TArray<FAnimNotifyEvent>& Notifies)
    {
        json::JSON NotifiesNode = json::JSON::Make(json::JSON::Class::Array);
        for (const FAnimNotifyEvent& Notify : Notifies)
        {
            json::JSON NotifyNode = json::JSON::Make(json::JSON::Class::Object);
            NotifyNode["TriggerTime"] = Notify.TriggerTime;
            NotifyNode["Duration"] = Notify.Duration;
            NotifyNode["NotifyName"] = Notify.NotifyName.ToString();
            NotifyNode["ActionType"] = static_cast<int32>(Notify.ActionType);
            NotifyNode["EventId"] = Notify.EventId.ToString();
            NotifyNode["Payload"] = Notify.Payload;
            NotifiesNode.append(NotifyNode);
        }

        Root["Notifies"] = NotifiesNode;
    }
}

bool FAnimSequenceAssetDescriptor::IsValid() const
{
    return !SourceFbxPath.empty() && !TargetSkeletalMeshPath.empty() && !AnimStackName.empty();
}

void FAnimSequenceAssetDescriptor::Serialize(FArchive& Ar)
{
    Ar << "AssetPath" << AssetPath;
    Ar << "SourceFbxPath" << SourceFbxPath;
    Ar << "TargetSkeletalMeshPath" << TargetSkeletalMeshPath;
    Ar << "AnimStackName" << AnimStackName;
}

bool FAnimSequenceAssetLoader::Load(const FString& Path, FAnimSequenceAssetDescriptor& OutDescriptor) const
{
    const FString NormalizedPath = NormalizeAnimSequenceAssetPath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
    {
        return false;
    }

    std::ifstream AssetFile(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath))));
    if (!AssetFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open anim sequence asset: %s", NormalizedPath.c_str());
        return false;
    }

    FString FileContent((std::istreambuf_iterator<char>(AssetFile)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(FileContent);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid anim sequence asset json: %s", NormalizedPath.c_str());
        return false;
    }

    FJsonReader Reader(Root);
    OutDescriptor = {};
    OutDescriptor.Serialize(Reader);
    if (!ReadNotifies(Root, NormalizedPath, OutDescriptor.Notifies, OutDescriptor.bHasAuthoredNotifies))
    {
        return false;
    }

    if (OutDescriptor.AssetPath.empty())
    {
        OutDescriptor.AssetPath = NormalizedPath;
    }

    if (!OutDescriptor.IsValid())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid anim sequence asset descriptor: %s", NormalizedPath.c_str());
        return false;
    }

    return true;
}

bool FAnimSequenceAssetLoader::Save(const FString& Path, const FAnimSequenceAssetDescriptor& Descriptor) const
{
    const FString NormalizedPath = NormalizeAnimSequenceAssetPath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath) || !Descriptor.IsValid())
    {
        return false;
    }

    FAnimSequenceAssetDescriptor WritableDescriptor = Descriptor;
    WritableDescriptor.AssetPath = NormalizedPath;

    json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
    FJsonWriter Writer(Root);
    WritableDescriptor.Serialize(Writer);
    if (WritableDescriptor.bHasAuthoredNotifies)
    {
        WriteNotifies(Root, WritableDescriptor.Notifies);
    }

    std::error_code ErrorCode;
    std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
    if (ErrorCode)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to create anim sequence asset directory: %s", NormalizedPath.c_str());
        return false;
    }

    std::ofstream OutFile(FilePath);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open anim sequence asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    OutFile << Root.dump(4);
    return true;
}

bool FAnimSequenceAssetLoader::SupportsExtension(const FString& Extension) const
{
    return Extension == ".animsequence" || Extension == "animsequence";
}

FString FAnimSequenceAssetLoader::GetLoaderName() const
{
    return "FAnimSequenceAssetLoader";
}
