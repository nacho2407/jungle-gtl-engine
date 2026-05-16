#include "Asset/AnimSequenceAssetLoader.h"

#include "Animation/AnimSequence.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

namespace
{
    using json::JSON;

    constexpr const char* AnimSequenceFormatName = "JSE.AnimSequence";
    constexpr int32 AnimSequenceFormatVersion = 1;

    FString NormalizeAnimSequencePath(const FString& Path)
    {
        return FPaths::Normalize(Path);
    }

    FString ToLowerCopy(FString Value)
    {
        std::transform(
            Value.begin(),
            Value.end(),
            Value.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Value;
    }

    bool IsAnimSequenceAssetPath(const FString& Path)
    {
        const FString LowerPath = ToLowerCopy(FPaths::Normalize(Path));
        const std::filesystem::path FsPath(FPaths::ToWide(LowerPath));
        const std::wstring Extension = FsPath.extension().wstring();
        return Extension == L".animseq" || Extension == L".sequence";
    }

    JSON MakeVector3Json(const FVector3f& Value)
    {
        JSON Result = JSON::Make(JSON::Class::Array);
        Result.append(Value.X);
        Result.append(Value.Y);
        Result.append(Value.Z);
        return Result;
    }

    JSON MakeQuatJson(const FQuat4f& Value)
    {
        JSON Result = JSON::Make(JSON::Class::Array);
        Result.append(Value.X);
        Result.append(Value.Y);
        Result.append(Value.Z);
        Result.append(Value.W);
        return Result;
    }

    FVector3f ReadVector3Json(JSON& Value, const FVector3f& Fallback = FVector3f::ZeroVector)
    {
        if (Value.JSONType() != JSON::Class::Array || Value.length() < 3)
        {
            return Fallback;
        }

        return FVector3f(
            static_cast<float>(Value[0].ToFloat()),
            static_cast<float>(Value[1].ToFloat()),
            static_cast<float>(Value[2].ToFloat()));
    }

    FQuat4f ReadQuatJson(JSON& Value, const FQuat4f& Fallback = FQuat4f::Identity)
    {
        if (Value.JSONType() != JSON::Class::Array || Value.length() < 4)
        {
            return Fallback;
        }

        FQuat4f Quat(
            static_cast<float>(Value[0].ToFloat()),
            static_cast<float>(Value[1].ToFloat()),
            static_cast<float>(Value[2].ToFloat()),
            static_cast<float>(Value[3].ToFloat()));
        return Quat.GetNormalized();
    }

    JSON MakeVectorKeyArrayJson(const TArray<FVector3f>& Keys)
    {
        JSON Result = JSON::Make(JSON::Class::Array);
        for (const FVector3f& Key : Keys)
        {
            Result.append(MakeVector3Json(Key));
        }
        return Result;
    }

    JSON MakeQuatKeyArrayJson(const TArray<FQuat4f>& Keys)
    {
        JSON Result = JSON::Make(JSON::Class::Array);
        for (const FQuat4f& Key : Keys)
        {
            Result.append(MakeQuatJson(Key));
        }
        return Result;
    }

    void ReadVectorKeyArrayJson(JSON& Value, TArray<FVector3f>& OutKeys, const FVector3f& Fallback = FVector3f::ZeroVector)
    {
        OutKeys.clear();
        if (Value.JSONType() != JSON::Class::Array)
        {
            return;
        }

        OutKeys.reserve(Value.length());
        for (int32 Index = 0; Index < static_cast<int32>(Value.length()); ++Index)
        {
            OutKeys.push_back(ReadVector3Json(Value[Index], Fallback));
        }
    }

    void ReadQuatKeyArrayJson(JSON& Value, TArray<FQuat4f>& OutKeys)
    {
        OutKeys.clear();
        if (Value.JSONType() != JSON::Class::Array)
        {
            return;
        }

        OutKeys.reserve(Value.length());
        for (int32 Index = 0; Index < static_cast<int32>(Value.length()); ++Index)
        {
            OutKeys.push_back(ReadQuatJson(Value[Index]));
        }
    }

    int32 GetIntOrDefault(JSON& Root, const char* Key, int32 DefaultValue)
    {
        return Root.hasKey(Key) ? static_cast<int32>(Root[Key].ToInt()) : DefaultValue;
    }

    float GetFloatOrDefault(JSON& Root, const char* Key, float DefaultValue)
    {
        return Root.hasKey(Key) ? static_cast<float>(Root[Key].ToFloat()) : DefaultValue;
    }

    FString GetStringOrDefault(JSON& Root, const char* Key, const FString& DefaultValue = "")
    {
        return Root.hasKey(Key) ? Root[Key].ToString() : DefaultValue;
    }

    JSON SerializeTrack(const FBoneAnimationTrack& Track)
    {
        JSON TrackJson = JSON::Make(JSON::Class::Object);
        TrackJson["BoneName"] = Track.Name.ToString();
        TrackJson["PosKeys"] = MakeVectorKeyArrayJson(Track.InternalTrack.PosKeys);
        TrackJson["RotKeys"] = MakeQuatKeyArrayJson(Track.InternalTrack.RotKeys);
        TrackJson["ScaleKeys"] = MakeVectorKeyArrayJson(Track.InternalTrack.ScaleKeys);
        return TrackJson;
    }

    void DeserializeTrack(JSON& TrackJson, FBoneAnimationTrack& OutTrack)
    {
        OutTrack = FBoneAnimationTrack();
        OutTrack.Name = FName(GetStringOrDefault(TrackJson, "BoneName"));

        if (TrackJson.hasKey("PosKeys"))
        {
            ReadVectorKeyArrayJson(TrackJson["PosKeys"], OutTrack.InternalTrack.PosKeys, FVector3f::ZeroVector);
        }
        if (TrackJson.hasKey("RotKeys"))
        {
            ReadQuatKeyArrayJson(TrackJson["RotKeys"], OutTrack.InternalTrack.RotKeys);
        }
        if (TrackJson.hasKey("ScaleKeys"))
        {
            ReadVectorKeyArrayJson(TrackJson["ScaleKeys"], OutTrack.InternalTrack.ScaleKeys, FVector3f::OneVector);
        }
    }
}

UAnimSequence* FAnimSequenceAssetLoader::Load(const FString& Path) const
{
    const FString NormalizedPath = NormalizeAnimSequencePath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
    {
        return nullptr;
    }

    std::ifstream AnimFile(FPaths::ToWide(NormalizedPath));
    if (!AnimFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open anim sequence asset: %s", NormalizedPath.c_str());
        return nullptr;
    }

    const FString FileContent((std::istreambuf_iterator<char>(AnimFile)), std::istreambuf_iterator<char>());
    JSON Root = JSON::Load(FileContent);
    if (Root.JSONType() != JSON::Class::Object)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid anim sequence json: %s", NormalizedPath.c_str());
        return nullptr;
    }

    const FString Format = GetStringOrDefault(Root, "Format");
    if (!Format.empty() && Format != AnimSequenceFormatName)
    {
        UE_LOG_WARNING("[AnimSequenceAssetLoader] Unexpected anim sequence format: %s | Path=%s", Format.c_str(), NormalizedPath.c_str());
    }

    UAnimSequence* Sequence = UObjectManager::Get().CreateObject<UAnimSequence>();
    UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
    Sequence->SetDataModel(DataModel);
    Sequence->SetAssetPath(NormalizedPath);
    Sequence->SetSourceFilePath(GetStringOrDefault(Root, "SourceFilePath"));
    Sequence->SetSourceStackName(GetStringOrDefault(Root, "SourceStackName"));
    Sequence->SetPreviewMeshPath(GetStringOrDefault(Root, "PreviewMeshPath"));

    FFrameRate FrameRate;
    FrameRate.Numerator = GetIntOrDefault(Root, "FrameRateNumerator", 30);
    FrameRate.Denominator = GetIntOrDefault(Root, "FrameRateDenominator", 1);
    DataModel->SetFrameRate(FrameRate);
    DataModel->SetPlayLength(GetFloatOrDefault(Root, "PlayLength", 0.0f));
    DataModel->SetNumberOfFrames(GetIntOrDefault(Root, "NumberOfFrames", 0));
    DataModel->SetNumberOfKeys(GetIntOrDefault(Root, "NumberOfKeys", 0));

    TArray<FBoneAnimationTrack>& Tracks = DataModel->GetMutableBoneAnimationTracks();
    Tracks.clear();
    if (Root.hasKey("Tracks") && Root["Tracks"].JSONType() == JSON::Class::Array)
    {
        JSON& TracksJson = Root["Tracks"];
        Tracks.reserve(TracksJson.length());
        for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(TracksJson.length()); ++TrackIndex)
        {
            if (TracksJson[TrackIndex].JSONType() != JSON::Class::Object)
            {
                continue;
            }

            FBoneAnimationTrack Track;
            DeserializeTrack(TracksJson[TrackIndex], Track);
            Tracks.push_back(std::move(Track));
        }
    }

    return Sequence;
}

bool FAnimSequenceAssetLoader::Save(const FString& Path, const UAnimSequence* Sequence) const
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        return false;
    }

    const FString NormalizedPath = NormalizeAnimSequencePath(Path);
    if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
    {
        return false;
    }

    const UAnimDataModel* DataModel = Sequence->GetDataModel();

    JSON Root = JSON::Make(JSON::Class::Object);
    Root["Format"] = AnimSequenceFormatName;
    Root["Version"] = AnimSequenceFormatVersion;
    Root["AssetPath"] = NormalizedPath;
    Root["SourceFilePath"] = Sequence->GetSourceFilePath();
    Root["SourceStackName"] = Sequence->GetSourceStackName();
    Root["PreviewMeshPath"] = Sequence->GetPreviewMeshPath();
    Root["PlayLength"] = DataModel->GetPlayLength();
    Root["FrameRateNumerator"] = DataModel->GetFrameRate().Numerator;
    Root["FrameRateDenominator"] = DataModel->GetFrameRate().Denominator;
    Root["NumberOfFrames"] = DataModel->GetNumberOfFrames();
    Root["NumberOfKeys"] = DataModel->GetNumberOfKeys();

    JSON TracksJson = JSON::Make(JSON::Class::Array);
    for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
    {
        TracksJson.append(SerializeTrack(Track));
    }
    Root["Tracks"] = TracksJson;

    std::error_code ErrorCode;
    const std::filesystem::path FilePath(FPaths::ToWide(NormalizedPath));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

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
    const FString LowerExtension = ToLowerCopy(Extension);
    return LowerExtension == ".animseq" || LowerExtension == "animseq" ||
           LowerExtension == ".sequence" || LowerExtension == "sequence";
}