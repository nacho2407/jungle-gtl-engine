#include "Asset/AnimSequenceAssetLoader.h"

#include "Animation/AnimSequence.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace
{
	using json::JSON;

	constexpr const char* AnimSequenceFormatName = "JSE.AnimSequence";
	constexpr int32 AnimSequenceFormatVersion = 1;

	// Companion cache format for .animseq. The JSON file remains the editable source,
	// and this cache is rebuilt whenever the source JSON timestamp changes.
	constexpr uint32 AnimSequenceBinaryMagic = 0x51455341; // 'ASEQ'
	constexpr uint32 AnimSequenceBinaryVersion = 1;

	constexpr uint32 MaxAnimSequenceTrackCount = 65'536;
	constexpr uint32 MaxAnimSequenceKeyCount = 1'000'000;
	constexpr uint32 MaxAnimSequenceStringLength = 65'536;

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


	FString MakeAnimSequenceBinaryCachePath(const FString& AnimSequencePath)
	{
		const std::filesystem::path SourcePath(FPaths::ToWide(FPaths::Normalize(AnimSequencePath)));
		const std::filesystem::path BinaryDirectory = SourcePath.parent_path() / L"Bin";

		std::filesystem::path BinaryFileName = SourcePath.filename();
		BinaryFileName += L".bin";

		return FPaths::ToUtf8((BinaryDirectory / BinaryFileName).generic_wstring());
	}

	uint64 GetFileWriteTimeTicks(const FString& Path)
	{
		namespace fs = std::filesystem;

		const fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(Path))));
		std::error_code ErrorCode;
		if (!fs::exists(FilePath, ErrorCode) || ErrorCode)
		{
			return 0;
		}

		const auto WriteTime = fs::last_write_time(FilePath, ErrorCode);
		if (ErrorCode)
		{
			return 0;
		}

		const auto Duration = WriteTime.time_since_epoch();
		return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
	}

	uint64 GetFileSizeBytes(const FString& Path)
	{
		namespace fs = std::filesystem;

		const fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(Path))));
		std::error_code ErrorCode;
		const uint64 Size = static_cast<uint64>(fs::file_size(FilePath, ErrorCode));
		return ErrorCode ? 0 : Size;
	}

	void WriteUInt32LE(std::ofstream& Out, uint32 Value)
	{
		const unsigned char Bytes[4] = {
			static_cast<unsigned char>((Value >> 0) & 0xFF),
			static_cast<unsigned char>((Value >> 8) & 0xFF),
			static_cast<unsigned char>((Value >> 16) & 0xFF),
			static_cast<unsigned char>((Value >> 24) & 0xFF)};
		Out.write(reinterpret_cast<const char*>(Bytes), sizeof(Bytes));
	}

	void WriteInt32LE(std::ofstream& Out, int32 Value)
	{
		WriteUInt32LE(Out, static_cast<uint32>(Value));
	}

	void WriteUInt64LE(std::ofstream& Out, uint64 Value)
	{
		unsigned char Bytes[8] = {};
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Bytes[Index] = static_cast<unsigned char>((Value >> (Index * 8)) & 0xFF);
		}
		Out.write(reinterpret_cast<const char*>(Bytes), sizeof(Bytes));
	}

	void WriteFloatLE(std::ofstream& Out, float Value)
	{
		static_assert(sizeof(float) == sizeof(uint32), "float must be 32-bit");
		uint32 Bits = 0;
		std::memcpy(&Bits, &Value, sizeof(float));
		WriteUInt32LE(Out, Bits);
	}

	bool ReadUInt32LE(std::ifstream& In, uint32& OutValue)
	{
		unsigned char Bytes[4] = {};
		In.read(reinterpret_cast<char*>(Bytes), sizeof(Bytes));
		if (!In.good())
		{
			return false;
		}

		OutValue =
			(static_cast<uint32>(Bytes[0]) << 0) |
			(static_cast<uint32>(Bytes[1]) << 8) |
			(static_cast<uint32>(Bytes[2]) << 16) |
			(static_cast<uint32>(Bytes[3]) << 24);
		return true;
	}

	bool ReadInt32LE(std::ifstream& In, int32& OutValue)
	{
		uint32 Bits = 0;
		if (!ReadUInt32LE(In, Bits))
		{
			return false;
		}
		OutValue = static_cast<int32>(Bits);
		return true;
	}

	bool ReadUInt64LE(std::ifstream& In, uint64& OutValue)
	{
		unsigned char Bytes[8] = {};
		In.read(reinterpret_cast<char*>(Bytes), sizeof(Bytes));
		if (!In.good())
		{
			return false;
		}

		OutValue = 0;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			OutValue |= (static_cast<uint64>(Bytes[Index]) << (Index * 8));
		}
		return true;
	}

	bool ReadFloatLE(std::ifstream& In, float& OutValue)
	{
		uint32 Bits = 0;
		if (!ReadUInt32LE(In, Bits))
		{
			return false;
		}
		std::memcpy(&OutValue, &Bits, sizeof(float));
		return true;
	}

	void WriteStringBinary(std::ofstream& Out, const FString& Value)
	{
		const uint32 Length = static_cast<uint32>((std::min)(
			Value.size(),
			static_cast<size_t>((std::numeric_limits<uint32>::max)())));
		WriteUInt32LE(Out, Length);
		if (Length > 0)
		{
			Out.write(Value.data(), Length);
		}
	}

	bool ReadStringBinary(std::ifstream& In, FString& OutValue)
	{
		uint32 Length = 0;
		if (!ReadUInt32LE(In, Length) || Length > MaxAnimSequenceStringLength)
		{
			OutValue.clear();
			return false;
		}

		OutValue.assign(Length, '\0');
		if (Length > 0)
		{
			In.read(OutValue.data(), Length);
			if (!In.good())
			{
				OutValue.clear();
				return false;
			}
		}
		return true;
	}

	void WriteVector3Binary(std::ofstream& Out, const FVector3f& Value)
	{
		WriteFloatLE(Out, Value.X);
		WriteFloatLE(Out, Value.Y);
		WriteFloatLE(Out, Value.Z);
	}

	bool ReadVector3Binary(std::ifstream& In, FVector3f& OutValue)
	{
		return ReadFloatLE(In, OutValue.X) &&
			   ReadFloatLE(In, OutValue.Y) &&
			   ReadFloatLE(In, OutValue.Z);
	}

	void WriteQuatBinary(std::ofstream& Out, const FQuat4f& Value)
	{
		WriteFloatLE(Out, Value.X);
		WriteFloatLE(Out, Value.Y);
		WriteFloatLE(Out, Value.Z);
		WriteFloatLE(Out, Value.W);
	}

	bool ReadQuatBinary(std::ifstream& In, FQuat4f& OutValue)
	{
		if (!ReadFloatLE(In, OutValue.X) ||
			!ReadFloatLE(In, OutValue.Y) ||
			!ReadFloatLE(In, OutValue.Z) ||
			!ReadFloatLE(In, OutValue.W))
		{
			return false;
		}

		OutValue = OutValue.GetNormalized();
		return true;
	}

	void WriteVectorKeyArrayBinary(std::ofstream& Out, const TArray<FVector3f>& Keys)
	{
		WriteUInt32LE(Out, static_cast<uint32>(Keys.size()));
		for (const FVector3f& Key : Keys)
		{
			WriteVector3Binary(Out, Key);
		}
	}

	bool ReadVectorKeyArrayBinary(std::ifstream& In, TArray<FVector3f>& OutKeys)
	{
		uint32 Count = 0;
		if (!ReadUInt32LE(In, Count) || Count > MaxAnimSequenceKeyCount)
		{
			return false;
		}

		OutKeys.clear();
		OutKeys.reserve(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FVector3f Key;
			if (!ReadVector3Binary(In, Key))
			{
				OutKeys.clear();
				return false;
			}
			OutKeys.push_back(Key);
		}
		return true;
	}

	void WriteQuatKeyArrayBinary(std::ofstream& Out, const TArray<FQuat4f>& Keys)
	{
		WriteUInt32LE(Out, static_cast<uint32>(Keys.size()));
		for (const FQuat4f& Key : Keys)
		{
			WriteQuatBinary(Out, Key);
		}
	}

	bool ReadQuatKeyArrayBinary(std::ifstream& In, TArray<FQuat4f>& OutKeys)
	{
		uint32 Count = 0;
		if (!ReadUInt32LE(In, Count) || Count > MaxAnimSequenceKeyCount)
		{
			return false;
		}

		OutKeys.clear();
		OutKeys.reserve(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FQuat4f Key;
			if (!ReadQuatBinary(In, Key))
			{
				OutKeys.clear();
				return false;
			}
			OutKeys.push_back(Key);
		}
		return true;
	}

	void WriteTrackBinary(std::ofstream& Out, const FBoneAnimationTrack& Track)
	{
		WriteStringBinary(Out, Track.Name.ToString());
		WriteVectorKeyArrayBinary(Out, Track.InternalTrack.PosKeys);
		WriteQuatKeyArrayBinary(Out, Track.InternalTrack.RotKeys);
		WriteVectorKeyArrayBinary(Out, Track.InternalTrack.ScaleKeys);
	}

	bool ReadTrackBinary(std::ifstream& In, FBoneAnimationTrack& OutTrack)
	{
		FString BoneName;
		if (!ReadStringBinary(In, BoneName))
		{
			return false;
		}

		OutTrack = FBoneAnimationTrack();
		OutTrack.Name = FName(BoneName);
		return ReadVectorKeyArrayBinary(In, OutTrack.InternalTrack.PosKeys) &&
			   ReadQuatKeyArrayBinary(In, OutTrack.InternalTrack.RotKeys) &&
			   ReadVectorKeyArrayBinary(In, OutTrack.InternalTrack.ScaleKeys);
	}

	bool SaveAnimSequenceBinaryCache(const FString& SourcePath, const UAnimSequence* Sequence)
	{
		if (!Sequence || !Sequence->GetDataModel())
		{
			return false;
		}

		const FString NormalizedSourcePath = NormalizeAnimSequencePath(SourcePath);
		const FString BinaryPath = MakeAnimSequenceBinaryCachePath(NormalizedSourcePath);
		const std::filesystem::path AbsoluteBinaryPath(FPaths::ToAbsolute(FPaths::ToWide(BinaryPath)));

		std::error_code ErrorCode;
		std::filesystem::create_directories(AbsoluteBinaryPath.parent_path(), ErrorCode);
		if (ErrorCode)
		{
			return false;
		}

		std::ofstream Out(AbsoluteBinaryPath, std::ios::binary);
		if (!Out.is_open())
		{
			return false;
		}

		const UAnimDataModel* DataModel = Sequence->GetDataModel();
		const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
		if (Tracks.size() > MaxAnimSequenceTrackCount)
		{
			return false;
		}

		for (const FBoneAnimationTrack& Track : Tracks)
		{
			if (Track.InternalTrack.PosKeys.size() > MaxAnimSequenceKeyCount ||
				Track.InternalTrack.RotKeys.size() > MaxAnimSequenceKeyCount ||
				Track.InternalTrack.ScaleKeys.size() > MaxAnimSequenceKeyCount)
			{
				return false;
			}
		}

		WriteUInt32LE(Out, AnimSequenceBinaryMagic);
		WriteUInt32LE(Out, AnimSequenceBinaryVersion);
		WriteUInt64LE(Out, GetFileWriteTimeTicks(NormalizedSourcePath));
		WriteUInt64LE(Out, GetFileSizeBytes(NormalizedSourcePath));
		WriteUInt32LE(Out, static_cast<uint32>(Tracks.size()));

		WriteFloatLE(Out, DataModel->GetPlayLength());
		WriteInt32LE(Out, DataModel->GetFrameRate().Numerator);
		WriteInt32LE(Out, DataModel->GetFrameRate().Denominator);
		WriteInt32LE(Out, DataModel->GetNumberOfFrames());
		WriteInt32LE(Out, DataModel->GetNumberOfKeys());

		WriteStringBinary(Out, Sequence->GetSourceFilePath());
		WriteStringBinary(Out, Sequence->GetSourceStackName());
		WriteStringBinary(Out, Sequence->GetPreviewMeshPath());

		for (const FBoneAnimationTrack& Track : Tracks)
		{
			WriteTrackBinary(Out, Track);
		}

		return Out.good();
	}

	UAnimSequence* LoadAnimSequenceBinaryCache(const FString& SourcePath)
	{
		const FString NormalizedSourcePath = NormalizeAnimSequencePath(SourcePath);
		const FString BinaryPath = MakeAnimSequenceBinaryCachePath(NormalizedSourcePath);
		const std::filesystem::path AbsoluteBinaryPath(FPaths::ToAbsolute(FPaths::ToWide(BinaryPath)));

		std::ifstream In(AbsoluteBinaryPath, std::ios::binary);
		if (!In.is_open())
		{
			return nullptr;
		}

		uint32 Magic = 0;
		uint32 Version = 0;
		uint64 CachedSourceWriteTime = 0;
		uint64 CachedSourceFileSize = 0;
		uint32 TrackCount = 0;
		if (!ReadUInt32LE(In, Magic) ||
			!ReadUInt32LE(In, Version) ||
			!ReadUInt64LE(In, CachedSourceWriteTime) ||
			!ReadUInt64LE(In, CachedSourceFileSize) ||
			!ReadUInt32LE(In, TrackCount))
		{
			return nullptr;
		}

		if (Magic != AnimSequenceBinaryMagic ||
			Version != AnimSequenceBinaryVersion ||
			TrackCount > MaxAnimSequenceTrackCount)
		{
			return nullptr;
		}

		const uint64 SourceWriteTime = GetFileWriteTimeTicks(NormalizedSourcePath);
		const uint64 SourceFileSize = GetFileSizeBytes(NormalizedSourcePath);
		if (SourceWriteTime == 0 ||
			SourceFileSize == 0 ||
			CachedSourceWriteTime != SourceWriteTime ||
			CachedSourceFileSize != SourceFileSize)
		{
			return nullptr;
		}

		float PlayLength = 0.0f;
		FFrameRate FrameRate;
		int32 NumberOfFrames = 0;
		int32 NumberOfKeys = 0;
		if (!ReadFloatLE(In, PlayLength) ||
			!ReadInt32LE(In, FrameRate.Numerator) ||
			!ReadInt32LE(In, FrameRate.Denominator) ||
			!ReadInt32LE(In, NumberOfFrames) ||
			!ReadInt32LE(In, NumberOfKeys))
		{
			return nullptr;
		}

		FString SourceFilePath;
		FString SourceStackName;
		FString PreviewMeshPath;
		if (!ReadStringBinary(In, SourceFilePath) ||
			!ReadStringBinary(In, SourceStackName) ||
			!ReadStringBinary(In, PreviewMeshPath))
		{
			return nullptr;
		}

		TArray<FBoneAnimationTrack> Tracks;
		Tracks.reserve(TrackCount);
		for (uint32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
		{
			FBoneAnimationTrack Track;
			if (!ReadTrackBinary(In, Track))
			{
				return nullptr;
			}
			Tracks.push_back(std::move(Track));
		}

		if (!In.good())
		{
			return nullptr;
		}

		UAnimSequence* Sequence = UObjectManager::Get().CreateObject<UAnimSequence>();
		UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
		Sequence->SetDataModel(DataModel);
		Sequence->SetAssetPath(NormalizedSourcePath);
		Sequence->SetSourceFilePath(SourceFilePath);
		Sequence->SetSourceStackName(SourceStackName);
		Sequence->SetPreviewMeshPath(PreviewMeshPath);

		DataModel->SetPlayLength(PlayLength);
		DataModel->SetFrameRate(FrameRate);
		DataModel->SetNumberOfFrames(NumberOfFrames);
		DataModel->SetNumberOfKeys(NumberOfKeys);
		DataModel->GetMutableBoneAnimationTracks() = std::move(Tracks);

		UE_LOG("[AnimSequenceAssetLoader] Loaded binary anim sequence cache: %s", BinaryPath.c_str());
		return Sequence;
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

	JSON SerializeTrackJson(const FBoneAnimationTrack& Track)
	{
		JSON Result = JSON::Make(JSON::Class::Object);
		Result["BoneName"] = Track.Name.ToString();
		Result["PosKeys"] = MakeVectorKeyArrayJson(Track.InternalTrack.PosKeys);
		Result["RotKeys"] = MakeQuatKeyArrayJson(Track.InternalTrack.RotKeys);
		Result["ScaleKeys"] = MakeVectorKeyArrayJson(Track.InternalTrack.ScaleKeys);
		return Result;
	}
}

UAnimSequence* FAnimSequenceAssetLoader::Load(const FString& Path) const
{
	const FString NormalizedPath = NormalizeAnimSequencePath(Path);
	if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
	{
		return nullptr;
	}

	if (UAnimSequence* BinarySequence = LoadAnimSequenceBinaryCache(NormalizedPath))
	{
		return BinarySequence;
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

	if (!SaveAnimSequenceBinaryCache(NormalizedPath, Sequence))
	{
		UE_LOG_WARNING("[AnimSequenceAssetLoader] Failed to build binary cache. JSON load will still be used next time: %s",
			NormalizedPath.c_str());
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
	OutFile.close();

	if (!SaveAnimSequenceBinaryCache(NormalizedPath, Sequence))
	{
		UE_LOG_WARNING("[AnimSequenceAssetLoader] Saved JSON but failed to save binary cache: %s", NormalizedPath.c_str());
	}

	return true;
}

bool FAnimSequenceAssetLoader::SupportsExtension(const FString& Extension) const
{
	const FString LowerExtension = ToLowerCopy(Extension);
	return LowerExtension == ".animseq" || LowerExtension == "animseq" ||
		   LowerExtension == ".sequence" || LowerExtension == "sequence";
}
