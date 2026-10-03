#pragma once

#include "Animation/AnimationTypes.h"
#include "Core/CoreMinimal.h"

#include <fstream>

struct FStaticMesh;
struct FSkeletalMesh;
struct FSkeletalMeshVertex;
struct FSkeletalMeshLODRenderData;
struct FReferenceSkeleton;
struct FMatrix;

namespace FAnimSequenceBinaryConstants
{
    constexpr uint32 Magic = 0x4D494E41; // 'ANIM'
    constexpr uint32 BinaryVersion = 9;  // v9: stores notify action payloads
    constexpr uint32 DerivedDataVersion = 2;  // v2: only authored FBX scale curves produce animated scale keys
}

struct FStaticMeshBinaryHeader
{
    uint32 MagicNumber = 0x4853454D;
    uint32 Version = 2;
    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
    uint32 SectionCount = 0;
    uint32 SlotCount = 0;

    uint64 SourceFileWriteTime = 0;
};

struct FSkeletalMeshBinaryHeader
{
    uint32 MagicNumber = 0x534D4B53; // 'SKMS'
    uint32 Version = 4;              // v4: LOD render data + section bone maps
    uint32 LODCount = 0;
    uint32 SlotCount = 0;
    uint32 BoneCount = 0;
    uint32 SocketCount = 0;

    uint64 SourceFileWriteTime = 0;
};

struct FAnimSequenceBinaryHeader
{
    uint32 Magic = FAnimSequenceBinaryConstants::Magic;
    uint32 BinaryVersion = FAnimSequenceBinaryConstants::BinaryVersion;
    uint32 DerivedDataVersion = FAnimSequenceBinaryConstants::DerivedDataVersion;
    uint64 SourceFileWriteTime = 0;
    uint64 SourceFileSize = 0;
    uint32 AnimStackNameHash = 0;
    uint32 TargetSkeletonBoneCount = 0;
    uint32 TargetSkeletonHash = 0;

    float SequenceLength = 0.0f;
    float FrameRate = 30.0f;
    int32 NumberOfFrames = 0;
    int32 TrackCount = 0;
};

class UAnimSequence;

class FBinarySerializer
{
public:
    bool SaveStaticMesh(const FString& BinaryPath, const FString& SourcePath, const FStaticMesh& Data);
    bool LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData);

    bool SaveSkeletalMesh(const FString& BinaryPath, const FString& SourcePath, const FSkeletalMesh& Data, const FReferenceSkeleton& ReferenceSkeleton);
    bool LoadSkeletalMesh(const FString& BinaryPath, FSkeletalMesh& OutData, FReferenceSkeleton& OutReferenceSkeleton);

    bool SaveAnimSequence(const FString& BinaryPath, const FString& SourcePath, const UAnimSequence& AnimSequence);
    bool LoadAnimSequence(const FString& BinaryPath, UAnimSequence& OutAnimSequence);
    bool ReadAnimSequenceHeader(const FString& BinaryPath, FAnimSequenceBinaryHeader& OutHeader) const;
    bool ReadAnimSequenceIdentity(
        const FString& BinaryPath,
        FAnimSequenceBinaryHeader& OutHeader,
        FString& OutSourceFbxPath,
        FString& OutTargetSkeletonPath,
        FString& OutAnimStackName) const;

    bool ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const;
    bool ReadSkeletalMeshHeader(const FString& BinaryPath, FSkeletalMeshBinaryHeader& OutHeader) const;

private:
    void WriteInt32LE(std::ofstream& Out, int32 Value);
    void WriteUInt32LE(std::ofstream& Out, uint32 Value);
    void WriteUInt64LE(std::ofstream& Out, uint64 Value);
    void WriteFloatLE(std::ofstream& Out, float Value);

    bool ReadInt32LE(std::ifstream& In, int32& OutValue) const;
    bool ReadUInt32LE(std::ifstream& In, uint32& OutValue) const;
    bool ReadUInt64LE(std::ifstream& In, uint64& OutValue) const;
    bool ReadFloatLE(std::ifstream& In, float& OutValue) const;

    void WriteHeader(std::ofstream& Out, const FStaticMeshBinaryHeader& Header);
    bool ReadHeader(std::ifstream& In, FStaticMeshBinaryHeader& OutHeader) const;

    void WriteString(std::ofstream& Out, const FString& String);
    bool ReadString(std::ifstream& In, FString& OutString) const;

    void WriteIndexArray(std::ofstream& Out, const TArray<uint32>& Array);
    bool ReadIndexArray(std::ifstream& In, TArray<uint32>& OutArray) const;

    void WriteVertices(std::ofstream& Out, const FStaticMesh& Data);
    bool ReadVertices(std::ifstream& In, FStaticMesh& OutData, uint32 VertexCount) const;

    void WriteSections(std::ofstream& Out, const FStaticMesh& Data);
    bool ReadSections(std::ifstream& In, FStaticMesh& OutData, uint32 SectionCount) const;

    void WriteBounds(std::ofstream& Out, const FStaticMesh& Data);
    bool ReadBounds(std::ifstream& In, FStaticMesh& OutData) const;

    void WriteSkeletalHeader(std::ofstream& Out, const FSkeletalMeshBinaryHeader& Header);
    bool ReadSkeletalHeader(std::ifstream& In, FSkeletalMeshBinaryHeader& OutHeader) const;

    void WriteMatrix4x4(std::ofstream& Out, const FMatrix& M);
    bool ReadMatrix4x4(std::ifstream& In, FMatrix& OutM) const;

    void WriteSkeletalVertices(std::ofstream& Out, const TArray<FSkeletalMeshVertex>& Vertices);
    bool ReadSkeletalVertices(std::ifstream& In, TArray<FSkeletalMeshVertex>& OutVertices, uint32 VertexCount) const;

    void WriteSkeletalLODRenderData(std::ofstream& Out, const FSkeletalMeshLODRenderData& LOD);
    bool ReadSkeletalLODRenderData(std::ifstream& In, FSkeletalMeshLODRenderData& OutLOD) const;

    void WriteBones(std::ofstream& Out, const FReferenceSkeleton& ReferenceSkeleton);
    bool ReadBones(std::ifstream& In, FReferenceSkeleton& OutReferenceSkeleton, uint32 BoneCount) const;

    void WriteSockets(std::ofstream& Out, const FSkeletalMesh& Data);
    bool ReadSockets(std::ifstream& In, FSkeletalMesh& OutData, uint32 SocketCount) const;

    void WriteSkeletalBounds(std::ofstream& Out, const FSkeletalMesh& Data);
    bool ReadSkeletalBounds(std::ifstream& In, FSkeletalMesh& OutData) const;

    void WriteAnimSequenceHeader(std::ofstream& Out, const FAnimSequenceBinaryHeader& Header);
    bool ReadAnimSequenceHeader(std::ifstream& In, FAnimSequenceBinaryHeader& OutHeader) const;
    void WriteAnimNotifies(std::ofstream& Out, const TArray<FAnimNotifyEvent>& Notifies);
    bool ReadAnimNotifies(std::ifstream& In, TArray<FAnimNotifyEvent>& OutNotifies) const;

    void WriteVector3(std::ofstream& Out, const FVector& V);
    bool ReadVector3(std::ifstream& In, FVector& OutV) const;

    void WriteQuat(std::ofstream& Out, const FQuat& Q);
    bool ReadQuat(std::ifstream& In, FQuat& OutQ) const;

    void WriteFloatArray(std::ofstream& Out, const TArray<float>& Array);
    bool ReadFloatArray(std::ifstream& In, TArray<float>& OutArray, uint32 MaxCount) const;

    void WriteVectorArray(std::ofstream& Out, const TArray<FVector>& Array);
    bool ReadVectorArray(std::ifstream& In, TArray<FVector>& OutArray, uint32 MaxCount) const;

    void WriteQuatArray(std::ofstream& Out, const TArray<FQuat>& Array);
    bool ReadQuatArray(std::ifstream& In, TArray<FQuat>& OutArray, uint32 MaxCount) const;
};
