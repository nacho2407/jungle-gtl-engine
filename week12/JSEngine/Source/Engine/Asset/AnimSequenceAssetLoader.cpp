#include "Asset/AnimSequenceAssetLoader.h"

#include "Animation/AnimNotify.h"
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
	constexpr int32 AnimSequenceFormatVersion = 2;

	// Companion cache format for .animseq. The JSON file remains the editable descriptor,
	// but fresh binary cache now contains all runtime data needed for playback.
	// v2 adds AnimNotify data so Load() can skip JSON parsing on cache hit.
	// v3 adds AnimNotify class names.
	constexpr uint32 AnimSequenceBinaryMagic = 0x51455341; // 'ASEQ'
	constexpr uint32 AnimSequenceBinaryVersion = 3;

	constexpr uint32 MaxAnimSequenceTrackCount = 65'536;
	constexpr uint32 MaxAnimSequenceKeyCount = 1'000'000;
	constexpr uint32 MaxAnimSequenceNotifyCount = 65'536;
	constexpr uint32 MaxAnimSequenceStringLength = 65'536;

	FString NormalizeAnimSequencePath(const FString& Path)
	{
		return FPaths::Normalize(Path);
	}

	FString MakeProjectRelativePath(const FString& Path)
	{
		const FString NormalizedPath = FPaths::Normalize(Path);
		if (NormalizedPath.empty())
		{
			return {};
		}

		std::filesystem::path FsPath(FPaths::ToWide(NormalizedPath));
		if (!FsPath.is_absolute())
		{
			return NormalizedPath;
		}

		std::error_code ErrorCode;
		const std::filesystem::path RelativePath = std::filesystem::relative(FsPath, std::filesystem::path(FPaths::RootDir()), ErrorCode);
		if (ErrorCode || RelativePath.empty())
		{
			return NormalizedPath;
		}

		const std::wstring RelativeText = RelativePath.generic_wstring();
		if (RelativeText == L".." || RelativeText.rfind(L"../", 0) == 0)
		{
			return NormalizedPath;
		}

		return FPaths::Normalize(FPaths::ToUtf8(RelativeText));
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

	uint64 ParseUInt64String(const FString& Value, uint64 DefaultValue = 0)
	{
		if (Value.empty())
		{
			return DefaultValue;
		}

		try
		{
			return static_cast<uint64>(std::stoull(Value));
		}
		catch (...)
		{
			return DefaultValue;
		}
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

	void WriteNotifyBinary(std::ofstream& Out, const FAnimNotifyEvent& Notify)
	{
		WriteFloatLE(Out, Notify.TriggerTime);
		WriteStringBinary(Out, Notify.NotifyName.ToString());
		WriteFloatLE(Out, Notify.Duration);
		WriteStringBinary(Out, Notify.NotifyClassName);
	}

	bool ReadNotifyBinary(std::ifstream& In, FAnimNotifyEvent& OutNotify)
	{
		FString NotifyName;
		FString NotifyClassName;
		if (!ReadFloatLE(In, OutNotify.TriggerTime) || !ReadStringBinary(In, NotifyName))
		{
			return false;
		}

		if (!ReadFloatLE(In, OutNotify.Duration) || !ReadStringBinary(In, NotifyClassName))
		{
			return false;
		}

		OutNotify.NotifyName = FName(NotifyName);
		OutNotify.NotifyClassName = NotifyClassName.empty()
			? UAnimNotify::GetDefaultNotifyClassName(OutNotify.IsState())
			: NotifyClassName;
		return true;
	}

	void WriteNotifyArrayBinary(std::ofstream& Out, const TArray<FAnimNotifyEvent>& Notifies)
	{
		WriteUInt32LE(Out, static_cast<uint32>(Notifies.size()));
		for (const FAnimNotifyEvent& Notify : Notifies)
		{
			WriteNotifyBinary(Out, Notify);
		}
	}

	bool ReadNotifyArrayBinary(std::ifstream& In, TArray<FAnimNotifyEvent>& OutNotifies)
	{
		uint32 Count = 0;
		if (!ReadUInt32LE(In, Count) || Count > MaxAnimSequenceNotifyCount)
		{
			return false;
		}

		OutNotifies.clear();
		OutNotifies.reserve(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FAnimNotifyEvent Notify;
			if (!ReadNotifyBinary(In, Notify))
			{
				OutNotifies.clear();
				return false;
			}
			OutNotifies.push_back(Notify);
		}

		return true;
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
		const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
		if (Tracks.size() > MaxAnimSequenceTrackCount || Notifies.size() > MaxAnimSequenceNotifyCount)
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
		WriteNotifyArrayBinary(Out, Notifies);

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

		TArray<FAnimNotifyEvent> Notifies;
		if (!ReadNotifyArrayBinary(In, Notifies))
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
		Sequence->SetDerivedDataCachePath(BinaryPath);
		Sequence->SetDerivedDataCacheVersion(static_cast<int32>(AnimSequenceBinaryVersion));
		Sequence->SetJsonTracksEmbedded(false);

		DataModel->SetPlayLength(PlayLength);
		DataModel->SetFrameRate(FrameRate);
		DataModel->SetNumberOfFrames(NumberOfFrames);
		DataModel->SetNumberOfKeys(NumberOfKeys);
		DataModel->GetMutableBoneAnimationTracks() = std::move(Tracks);

		for (const FAnimNotifyEvent& Notify : Notifies)
		{
			Sequence->AddNotify(Notify.TriggerTime, Notify.NotifyName, Notify.Duration, Notify.NotifyClassName);
		}

		UE_LOG("[AnimSequenceAssetLoader] Loaded binary anim sequence cache: %s", BinaryPath.c_str());
		return Sequence;
	}

	bool HasFreshAnimSequenceBinaryCache(const FString& SourcePath)
	{
		const FString NormalizedSourcePath = NormalizeAnimSequencePath(SourcePath);
		const FString BinaryPath = MakeAnimSequenceBinaryCachePath(NormalizedSourcePath);
		const std::filesystem::path AbsoluteBinaryPath(FPaths::ToAbsolute(FPaths::ToWide(BinaryPath)));

		std::ifstream In(AbsoluteBinaryPath, std::ios::binary);
		if (!In.is_open())
		{
			return false;
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
			return false;
		}

		if (Magic != AnimSequenceBinaryMagic ||
			Version != AnimSequenceBinaryVersion ||
			TrackCount > MaxAnimSequenceTrackCount)
		{
			return false;
		}

		const uint64 SourceWriteTime = GetFileWriteTimeTicks(NormalizedSourcePath);
		const uint64 SourceFileSize = GetFileSizeBytes(NormalizedSourcePath);
		return SourceWriteTime != 0 &&
			SourceFileSize != 0 &&
			CachedSourceWriteTime == SourceWriteTime &&
			CachedSourceFileSize == SourceFileSize;
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

	uint64 GetUInt64StringOrDefault(JSON& Root, const char* Key, uint64 DefaultValue = 0)
	{
		return Root.hasKey(Key) ? ParseUInt64String(Root[Key].ToString(), DefaultValue) : DefaultValue;
	}

	bool GetBoolOrDefault(JSON& Root, const char* Key, bool bDefaultValue = false)
	{
		return Root.hasKey(Key) ? Root[Key].ToBool() : bDefaultValue;
	}

	JSON* GetObjectOrNull(JSON& Root, const char* Key)
	{
		if (!Root.hasKey(Key) || Root[Key].JSONType() != JSON::Class::Object)
		{
			return nullptr;
		}

		return &Root[Key];
	}

	void ApplyAnimSequenceDescriptorMetadata(JSON& Root, const FString& NormalizedPath, UAnimSequence* Sequence)
	{
		if (!Sequence)
		{
			return;
		}

		Sequence->SetAssetPath(NormalizedPath);

		FString SourceFilePath = GetStringOrDefault(Root, "SourceFilePath");
		FString SourceStackName = GetStringOrDefault(Root, "SourceStackName");
		FString PreviewMeshPath = GetStringOrDefault(Root, "PreviewMeshPath");
		uint64 SourceFileWriteTimeTicks = 0;
		uint64 SourceFileSizeBytes = 0;
		FString SourceFileContentHash;

		if (JSON* Source = GetObjectOrNull(Root, "Source"))
		{
			SourceFilePath = GetStringOrDefault(*Source, "FilePath", SourceFilePath);
			SourceStackName = GetStringOrDefault(*Source, "ImportedStackName", SourceStackName);
			SourceFileWriteTimeTicks = GetUInt64StringOrDefault(*Source, "FileWriteTimeTicks", SourceFileWriteTimeTicks);
			SourceFileSizeBytes = GetUInt64StringOrDefault(*Source, "FileSizeBytes", SourceFileSizeBytes);
			SourceFileContentHash = GetStringOrDefault(*Source, "ContentHash", SourceFileContentHash);
		}

		if (JSON* ImportSettings = GetObjectOrNull(Root, "ImportSettings"))
		{
			PreviewMeshPath = GetStringOrDefault(*ImportSettings, "PreviewMeshPath", PreviewMeshPath);
		}

		if (JSON* DerivedData = GetObjectOrNull(Root, "DerivedData"))
		{
			Sequence->SetDerivedDataCachePath(GetStringOrDefault(
				*DerivedData,
				"CachePath",
				MakeAnimSequenceBinaryCachePath(NormalizedPath)));
			Sequence->SetDerivedDataCacheVersion(GetIntOrDefault(
				*DerivedData,
				"CacheVersion",
				static_cast<int32>(AnimSequenceBinaryVersion)));
			Sequence->SetJsonTracksEmbedded(GetBoolOrDefault(*DerivedData, "bJsonEmbedsRawTracks", Root.hasKey("Tracks")));
		}
		else
		{
			Sequence->SetDerivedDataCachePath(MakeAnimSequenceBinaryCachePath(NormalizedPath));
			Sequence->SetDerivedDataCacheVersion(static_cast<int32>(AnimSequenceBinaryVersion));
			Sequence->SetJsonTracksEmbedded(Root.hasKey("Tracks"));
		}

		if (!SourceFilePath.empty())
		{
			const FString NormalizedSourceFilePath = MakeProjectRelativePath(SourceFilePath);
			Sequence->SetSourceFilePath(NormalizedSourceFilePath);
			if (SourceFileWriteTimeTicks == 0)
			{
				SourceFileWriteTimeTicks = GetFileWriteTimeTicks(NormalizedSourceFilePath);
			}
			if (SourceFileSizeBytes == 0)
			{
				SourceFileSizeBytes = GetFileSizeBytes(NormalizedSourceFilePath);
			}
		}
		else
		{
			Sequence->SetSourceFilePath("");
		}

		Sequence->SetSourceStackName(SourceStackName);
		Sequence->SetPreviewMeshPath(MakeProjectRelativePath(PreviewMeshPath));
		Sequence->SetSourceFileWriteTimeTicks(SourceFileWriteTimeTicks);
		Sequence->SetSourceFileSizeBytes(SourceFileSizeBytes);
		Sequence->SetSourceFileContentHash(SourceFileContentHash);
	}

	void ApplyAnimSequenceDescriptorDataModel(JSON& Root, UAnimDataModel* DataModel)
	{
		if (!DataModel)
		{
			return;
		}

		FFrameRate FrameRate;
		FrameRate.Numerator = GetIntOrDefault(Root, "FrameRateNumerator", 30);
		FrameRate.Denominator = GetIntOrDefault(Root, "FrameRateDenominator", 1);

		float PlayLength = GetFloatOrDefault(Root, "PlayLength", 0.0f);
		int32 NumberOfFrames = GetIntOrDefault(Root, "NumberOfFrames", 0);
		int32 NumberOfKeys = GetIntOrDefault(Root, "NumberOfKeys", 0);

		if (JSON* ImportSettings = GetObjectOrNull(Root, "ImportSettings"))
		{
			FrameRate.Numerator = GetIntOrDefault(*ImportSettings, "FrameRateNumerator", FrameRate.Numerator);
			FrameRate.Denominator = GetIntOrDefault(*ImportSettings, "FrameRateDenominator", FrameRate.Denominator);
			FrameRate.Numerator = GetIntOrDefault(*ImportSettings, "SampleRate", FrameRate.Numerator);
		}

		if (JSON* Sequence = GetObjectOrNull(Root, "Sequence"))
		{
			PlayLength = GetFloatOrDefault(*Sequence, "PlayLength", PlayLength);
			NumberOfFrames = GetIntOrDefault(*Sequence, "NumberOfFrames", NumberOfFrames);
			NumberOfKeys = GetIntOrDefault(*Sequence, "NumberOfKeys", NumberOfKeys);
		}

		DataModel->SetFrameRate(FrameRate);
		DataModel->SetPlayLength(PlayLength);
		DataModel->SetNumberOfFrames(NumberOfFrames);
		DataModel->SetNumberOfKeys(NumberOfKeys);
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

	void ApplyAnimSequenceDescriptorNotifies(JSON& Root, UAnimSequenceBase* Sequence)
	{
		if (!Sequence)
		{
			return;
		}

		Sequence->ClearNotifies();
		if (!Root.hasKey("Notifies") || Root["Notifies"].JSONType() != JSON::Class::Array)
		{
			return;
		}

		JSON& NotifiesJson = Root["Notifies"];
		for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(NotifiesJson.length()); ++NotifyIndex)
		{
			if (NotifiesJson[NotifyIndex].JSONType() != JSON::Class::Object)
			{
				continue;
			}

			JSON& NotifyJson = NotifiesJson[NotifyIndex];
			FString NotifyName = GetStringOrDefault(NotifyJson, "NotifyName");
			if (NotifyName.empty())
			{
				NotifyName = GetStringOrDefault(NotifyJson, "Name");
			}
			if (NotifyName.empty())
			{
				NotifyName = "AnimNotify";
			}

			float TriggerTime = GetFloatOrDefault(NotifyJson, "TriggerTime", 0.0f);
			if (NotifyJson.hasKey("Time"))
			{
				TriggerTime = GetFloatOrDefault(NotifyJson, "Time", TriggerTime);
			}

			float Duration = GetFloatOrDefault(NotifyJson, "Duration", 0.0f);
			if (NotifyJson.hasKey("EndTime"))
			{
				Duration = GetFloatOrDefault(NotifyJson, "EndTime", TriggerTime) - TriggerTime;
			}

			FString NotifyClassName = GetStringOrDefault(NotifyJson, "NotifyClassName");
			if (NotifyClassName.empty())
			{
				NotifyClassName = GetStringOrDefault(NotifyJson, "NotifyClass");
			}
			if (NotifyClassName.empty())
			{
				NotifyClassName = UAnimNotify::GetDefaultNotifyClassName(Duration > 0.0f);
			}

			Sequence->AddNotify(TriggerTime, FName(NotifyName), Duration, NotifyClassName);
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

	// Fast path: .animseq JSON은 편집 가능한 descriptor이고, Bin/*.bin은 재생용 raw data cache다.
	// v2 binary에는 tracks와 notifies가 모두 들어있으므로 fresh cache라면 JSON을 열고 파싱하지 않는다.
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
	ApplyAnimSequenceDescriptorMetadata(Root, NormalizedPath, Sequence);
	ApplyAnimSequenceDescriptorDataModel(Root, DataModel);
	ApplyAnimSequenceDescriptorNotifies(Root, Sequence);

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

	if (!Tracks.empty() && !SaveAnimSequenceBinaryCache(NormalizedPath, Sequence))
	{
		UE_LOG_WARNING("[AnimSequenceAssetLoader] Failed to build binary cache from embedded JSON tracks: %s",
			NormalizedPath.c_str());
	}

	return Sequence;
}

bool FAnimSequenceAssetLoader::LoadMetadata(const FString& Path, FAnimSequenceAssetMetadata& OutMetadata) const
{
	OutMetadata = FAnimSequenceAssetMetadata{};

	const FString NormalizedPath = NormalizeAnimSequencePath(Path);
	if (NormalizedPath.empty() || !IsAnimSequenceAssetPath(NormalizedPath))
	{
		return false;
	}

	std::ifstream AnimFile(FPaths::ToWide(NormalizedPath));
	if (!AnimFile.is_open())
	{
		return false;
	}

	const FString FileContent((std::istreambuf_iterator<char>(AnimFile)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(FileContent);
	if (Root.JSONType() != JSON::Class::Object)
	{
		return false;
	}

	OutMetadata.SourceFilePath = GetStringOrDefault(Root, "SourceFilePath");
	OutMetadata.SourceStackName = GetStringOrDefault(Root, "SourceStackName");
	OutMetadata.PreviewMeshPath = GetStringOrDefault(Root, "PreviewMeshPath");
	OutMetadata.NumberOfKeys = GetIntOrDefault(Root, "NumberOfKeys", 0);

	if (JSON* Source = GetObjectOrNull(Root, "Source"))
	{
		OutMetadata.SourceFilePath = GetStringOrDefault(*Source, "FilePath", OutMetadata.SourceFilePath);
		OutMetadata.SourceStackName = GetStringOrDefault(*Source, "ImportedStackName", OutMetadata.SourceStackName);
		OutMetadata.SourceFileWriteTimeTicks = GetUInt64StringOrDefault(*Source, "FileWriteTimeTicks", 0);
		OutMetadata.SourceFileSizeBytes = GetUInt64StringOrDefault(*Source, "FileSizeBytes", 0);
		OutMetadata.SourceFileContentHash = GetStringOrDefault(*Source, "ContentHash");
	}

	if (JSON* ImportSettings = GetObjectOrNull(Root, "ImportSettings"))
	{
		OutMetadata.PreviewMeshPath = GetStringOrDefault(*ImportSettings, "PreviewMeshPath", OutMetadata.PreviewMeshPath);
	}

	if (JSON* Sequence = GetObjectOrNull(Root, "Sequence"))
	{
		OutMetadata.TrackCount = GetIntOrDefault(*Sequence, "TrackCount", OutMetadata.TrackCount);
		OutMetadata.NumberOfKeys = GetIntOrDefault(*Sequence, "NumberOfKeys", OutMetadata.NumberOfKeys);
	}
	else if (Root.hasKey("Tracks") && Root["Tracks"].JSONType() == JSON::Class::Array)
	{
		OutMetadata.TrackCount = static_cast<int32>(Root["Tracks"].length());
	}

	OutMetadata.SourceFilePath = MakeProjectRelativePath(OutMetadata.SourceFilePath);
	OutMetadata.PreviewMeshPath = MakeProjectRelativePath(OutMetadata.PreviewMeshPath);
	return true;
}

bool FAnimSequenceAssetLoader::HasValidBinaryCache(const FString& Path) const
{
	const FString NormalizedPath = NormalizeAnimSequencePath(Path);
	return !NormalizedPath.empty() && IsAnimSequenceAssetPath(NormalizedPath) && HasFreshAnimSequenceBinaryCache(NormalizedPath);
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

	const FString StableAssetPath = MakeProjectRelativePath(NormalizedPath);
	const FString SourceFilePath = MakeProjectRelativePath(Sequence->GetSourceFilePath());
	const FString PreviewMeshPath = MakeProjectRelativePath(Sequence->GetPreviewMeshPath());
	const FString DerivedDataCachePath = MakeProjectRelativePath(MakeAnimSequenceBinaryCachePath(StableAssetPath));

	JSON Root = JSON::Make(JSON::Class::Object);
	Root["Format"] = AnimSequenceFormatName;
	Root["Version"] = AnimSequenceFormatVersion;
	Root["AssetPath"] = StableAssetPath;

	// Backward-compatible flat fields. New code should prefer the grouped descriptor blocks below.
	Root["SourceFilePath"] = SourceFilePath;
	Root["SourceStackName"] = Sequence->GetSourceStackName();
	Root["PreviewMeshPath"] = PreviewMeshPath;
	Root["PlayLength"] = DataModel->GetPlayLength();
	Root["FrameRateNumerator"] = DataModel->GetFrameRate().Numerator;
	Root["FrameRateDenominator"] = DataModel->GetFrameRate().Denominator;
	Root["NumberOfFrames"] = DataModel->GetNumberOfFrames();
	Root["NumberOfKeys"] = DataModel->GetNumberOfKeys();

	JSON Source = JSON::Make(JSON::Class::Object);
	Source["FilePath"] = SourceFilePath;
	Source["ImportedStackName"] = Sequence->GetSourceStackName();
	Root["Source"] = Source;

	JSON ImportSettings = JSON::Make(JSON::Class::Object);
	ImportSettings["SampleRate"] = DataModel->GetFrameRate().Numerator;
	ImportSettings["FrameRateNumerator"] = DataModel->GetFrameRate().Numerator;
	ImportSettings["FrameRateDenominator"] = DataModel->GetFrameRate().Denominator;
	ImportSettings["PreviewMeshPath"] = PreviewMeshPath;
	Root["ImportSettings"] = ImportSettings;

	JSON SequenceJson = JSON::Make(JSON::Class::Object);
	SequenceJson["PlayLength"] = DataModel->GetPlayLength();
	SequenceJson["NumberOfFrames"] = DataModel->GetNumberOfFrames();
	SequenceJson["NumberOfKeys"] = DataModel->GetNumberOfKeys();
	SequenceJson["TrackCount"] = static_cast<int32>(DataModel->GetBoneAnimationTracks().size());
	SequenceJson["NotifyCount"] = static_cast<int32>(Sequence->GetNotifies().size());
	Root["Sequence"] = SequenceJson;

	JSON NotifiesJson = JSON::Make(JSON::Class::Array);
	for (const FAnimNotifyStateEvent& Notify : Sequence->GetNotifies())
	{
		JSON NotifyJson = JSON::Make(JSON::Class::Object);
		NotifyJson["NotifyName"] = Notify.NotifyName.ToString();
		NotifyJson["NotifyClassName"] = Notify.NotifyClassName;
		NotifyJson["TriggerTime"] = Notify.TriggerTime;
		NotifyJson["Duration"] = Notify.Duration;
		NotifyJson["EndTime"] = Notify.GetEndTime();
		NotifiesJson.append(NotifyJson);
	}
	Root["Notifies"] = NotifiesJson;

	JSON DerivedData = JSON::Make(JSON::Class::Object);
	DerivedData["Type"] = "SampledTrackBinaryCache";
	DerivedData["CachePath"] = DerivedDataCachePath;
	DerivedData["CacheFormat"] = "ASEQ";
	DerivedData["CacheVersion"] = static_cast<int32>(AnimSequenceBinaryVersion);
	DerivedData["bJsonEmbedsRawTracks"] = false;
	DerivedData["bCacheStoresRawTracks"] = true;
	Root["DerivedData"] = DerivedData;

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
