#include "Core/CoreMinimal.h"
#include "StaticMeshLoader.h"
#include "Asset/StaticMesh.h"
#include "Asset/UAssetBinary.h"
#include "Core/Serialization/WindowsBinArchive.h"
#include "Asset/MaterialLibrary.h"
#include "Asset/MaterialInterface.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/Types/VertexTypes.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
    bool IsUAssetOfType(const FWString& Path, EUAssetBinaryType Expected)
    {
        const fs::path FilePath(Path);
        std::error_code ErrorCode;
        if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
        {
            return false;
        }

        FWindowsBinReader Ar(FilePath);
        if (!Ar.IsValid())
        {
            return false;
        }

        FUAssetBinaryHeader Header;
        Header.Magic = 0;
        Header.Version = 0;
        uint32 TypeValue = 0;
        Header.AssetType = EUAssetBinaryType::Unknown;
        Header.SourceHash = 0;

        Ar << Header.Magic;
        Ar << Header.Version;
        Ar << TypeValue;
        Header.AssetType = static_cast<EUAssetBinaryType>(TypeValue);
        Ar << Header.SourceHash;

        if (Header.Magic != kUAssetMagic || Header.Version != kUAssetVersion)
        {
            return false;
        }

        return Header.AssetType == Expected;
    }
}

FStaticMeshLoader::FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager)
    : RHI(InRHI), AssetManager(InAssetManager)
{
}

bool FStaticMeshLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::StaticMesh)
    {
        return false;
    }

    if (HasExtension(Path, {L".obj"}))
    {
        return true;
    }

    if (HasExtension(Path, {L".uasset"}))
    {
        return IsUAssetOfType(Path, EUAssetBinaryType::StaticMesh);
    }

    return false;
}

EAssetType FStaticMeshLoader::GetAssetType() const { return EAssetType::StaticMesh; }

UAsset* FStaticMeshLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    std::shared_ptr<FStaticMesh> MeshResource = std::make_shared<FStaticMesh>();
    MeshResource->Reset();

    TArray<FMeshVertexPNCT> ParsedVertices;
    TArray<FString>         MaterialAssetPaths;

    const bool bIsUAsset = HasExtension(Source.NormalizedPath, {L".uasset"});

    // 1. OBJ ???????? ??CPU ????????
    if (bIsUAsset)
    {
        if (!ParseUAsset(Source, *MeshResource, ParsedVertices, MaterialAssetPaths))
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to parse .uasset file. Path=%s",
                   WidePathToUtf8(Source.NormalizedPath).c_str());
            return nullptr;
        }
    }
    else
    {
        if (!ParseObjText(Source, *MeshResource, ParsedVertices))
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to parse .obj file. Path=%s",
                   WidePathToUtf8(Source.NormalizedPath).c_str());
            return nullptr;
        }
    }

    // 2. 파싱된 데이터를 바탕으로 GPU 버퍼(VRAM) 생성
    if (!CreateBuffers(ParsedVertices, *MeshResource))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[StaticMeshLoader] Failed to create D3D11 buffers. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return nullptr;
    }

    // 3. 에셋 생성 및 리소스 복사
    Engine::Asset::UStaticMesh* NewMeshAsset = new Engine::Asset::UStaticMesh();
    NewMeshAsset->Initialize(Source, MeshResource);

    if (AssetManager && bIsUAsset)
    {
        FAssetId Id;
        Id.Type = EAssetType::StaticMesh;
        Id.PackageName = WidePathToUtf8(Source.NormalizedPath);
        const std::filesystem::path FilePath(Source.NormalizedPath);
        Id.ObjectName = WidePathToUtf8(FilePath.stem().native());
        AssetManager->RegisterAssetByIdAlias(Id, NewMeshAsset);
    }

    // --- 추가: 서브 메시 개수에 맞춰 머티리얼 슬롯 미리 확보 ---
    const uint32 NumSubMeshes = static_cast<uint32>(MeshResource->SubMeshes.size());
    NewMeshAsset->InitializeMaterialSlots(NumSubMeshes);

    if (!MaterialAssetPaths.empty())
    {
        ApplyMaterialAssetPaths(*NewMeshAsset, MaterialAssetPaths);
    }
        // .mtl 파일 로드 및 매핑
    else if (AssetManager && !MeshResource->MaterialLibraryPath.empty())
    {
        const fs::path ObjFilePath(Source.NormalizedPath);
        const fs::path ObjDir = ObjFilePath.parent_path();

        FWString MtlRelativeWidePath = NarrowPathToWidePath(MeshResource->MaterialLibraryPath);
        if (MtlRelativeWidePath.empty())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to decode mtllib path. RawPath=%s",
                   MeshResource->MaterialLibraryPath.c_str());
            return NewMeshAsset;
        }

        NormalizePathSeparators(MtlRelativeWidePath);

        FWString WideMtlPath;
        if (IsAbsoluteWidePath(MtlRelativeWidePath))
        {
            WideMtlPath = MtlRelativeWidePath;
        }
        else
        {
            FWString ObjDirectory = ObjDir.native();
            NormalizePathSeparators(ObjDirectory);
            if (!ObjDirectory.empty() && ObjDirectory.back() != L'\\')
            {
                ObjDirectory += L'\\';
            }
            WideMtlPath = ObjDirectory + MtlRelativeWidePath;
        }

        FAssetLoadParams MatParams;
        MatParams.ExplicitType = EAssetType::Material;

        UAsset* LoadedAsset = AssetManager->Load(WideMtlPath, MatParams);
        auto*   LoadedLibrary = Cast<Engine::Asset::UMaterialLibrary>(LoadedAsset);
        if (!LoadedLibrary)
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to load .mtl file. Path=%s",
                   WidePathToUtf8(WideMtlPath).c_str());
            return NewMeshAsset;
        }

        const auto& MaterialRefs = LoadedLibrary->GetMaterialRefs();
        for (uint32 i = 0; i < NumSubMeshes; ++i)
        {
            const FString& TargetMatName = MeshResource->SubMeshes[i].DefaultMaterialName;
            auto           RefIt = MaterialRefs.find(TargetMatName);
            if (RefIt != MaterialRefs.end())
            {
                if (UAsset* SubMaterialAsset = AssetManager->FindAssetById(RefIt->second))
                {
                    if (auto* MaterialInterface =
                            Cast<Engine::Asset::UMaterialInterface>(SubMaterialAsset))
                    {
                        NewMeshAsset->SetMaterialSlot(i, MaterialInterface);
                        continue;
                    }
                }
            }
            UE_LOG(Asset, ELogVerbosity::Log,
                   "[StaticMeshLoader] Material '%s' not found in .mtl '%s'",
                   TargetMatName.c_str(), WidePathToUtf8(WideMtlPath).c_str());
        }
    }
    return NewMeshAsset;
}

bool FStaticMeshLoader::ParseObjText(const FSourceRecord& Source, FStaticMesh& OutMesh,
                                     TArray<FMeshVertexPNCT>& OutVertices) const
{
    if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
        return false;

    // 1. 파일 전체를 감싸는 View 생성 (메모리 복사 없음)
    std::string_view FileView(reinterpret_cast<const char*>(Source.FileBytes.data()),
                              Source.FileBytes.size());

    std::vector<FVector>  TempPositions;
    std::vector<FVector2> TempUVs;
    std::vector<FVector>  TempNormals;

    // 2. TMap<FString, ...> 대신 string_view를 키로 사용하여 캐시에서도 문자열 복사를 방지
    std::unordered_map<std::string_view, uint32> VertexCache;

    FSubMesh CurrentSubMesh;
    bool     bSubMeshActive = false;

    // --- 파싱 헬퍼 함수들 ---

    // 토큰(단어)을 하나씩 잘라내는 함수 (공백 무시)
    auto GetNextToken = [](std::string_view& View) -> std::string_view
    {
        size_t First = View.find_first_not_of(" \t\r");
        if (First == std::string_view::npos)
            return {};
        View.remove_prefix(First);

        size_t           Last = View.find_first_of(" \t\r");
        std::string_view Token = View.substr(0, Last);
        if (Last != std::string_view::npos)
            View.remove_prefix(Last);
        else
            View = {}; // 마지막 토큰인 경우
        return Token;
    };

    // 빠른 float 파싱 (std::from_chars)
    auto ParseFloat = [](std::string_view Token, float& OutVal)
    {
        if (!Token.empty())
            std::from_chars(Token.data(), Token.data() + Token.size(), OutVal);
    };

    // 빠른 int 파싱
    auto ParseInt = [](std::string_view Token, int& OutVal)
    {
        if (!Token.empty())
            std::from_chars(Token.data(), Token.data() + Token.size(), OutVal);
    };

    // 앞뒤 공백을 제거한 1줄 파싱
    auto TrimView = [](std::string_view View) -> std::string_view
    {
        const size_t First = View.find_first_not_of(" \t\r");
        if (First == std::string_view::npos)
        {
            return {};
        }
        const size_t Last = View.find_last_not_of(" \t\r");
        return View.substr(First, Last - First + 1);
    };

    // --- 메인 파싱 루프 ---
    bool   bHasBounds = false;
    size_t LineStart = 0;
    while (LineStart < FileView.length())
    {
        // 줄바꿈 문자를 찾아 한 줄 단위의 View 생성
        size_t LineEnd = FileView.find('\n', LineStart);
        if (LineEnd == std::string_view::npos)
            LineEnd = FileView.length();

        std::string_view LineView = FileView.substr(LineStart, LineEnd - LineStart);
        LineStart = LineEnd + 1; // 다음 줄을 위해 전진

        // 헤더 추출
        std::string_view Header = GetNextToken(LineView);
        if (Header.empty() || Header[0] == '#')
            continue;

        if (Header == "v")
        {
            FVector Pos = {0.f, 0.f, 0.f};
            ParseFloat(GetNextToken(LineView), Pos.X);
            ParseFloat(GetNextToken(LineView), Pos.Y);
            ParseFloat(GetNextToken(LineView), Pos.Z);
            TempPositions.push_back(Pos);

            if (!bHasBounds)
            {
                OutMesh.BoundingBox.Min = Pos;
                OutMesh.BoundingBox.Max = Pos;
                bHasBounds = true;
            }
            else
            {
                OutMesh.BoundingBox.Min.X = std::min(OutMesh.BoundingBox.Min.X, Pos.X);
                OutMesh.BoundingBox.Min.Y = std::min(OutMesh.BoundingBox.Min.Y, Pos.Y);
                OutMesh.BoundingBox.Min.Z = std::min(OutMesh.BoundingBox.Min.Z, Pos.Z);
                OutMesh.BoundingBox.Max.X = std::max(OutMesh.BoundingBox.Max.X, Pos.X);
                OutMesh.BoundingBox.Max.Y = std::max(OutMesh.BoundingBox.Max.Y, Pos.Y);
                OutMesh.BoundingBox.Max.Z = std::max(OutMesh.BoundingBox.Max.Z, Pos.Z);
            }
        }
        else if (Header == "vt")
        {
            FVector2 UV = {0.f, 0.f};
            ParseFloat(GetNextToken(LineView), UV.X);
            ParseFloat(GetNextToken(LineView), UV.Y);
            UV.Y = 1.0f - UV.Y;
            TempUVs.push_back(UV);
        }
        else if (Header == "vn")
        {
            FVector Normal = {0.f, 0.f, 0.f};
            ParseFloat(GetNextToken(LineView), Normal.X);
            ParseFloat(GetNextToken(LineView), Normal.Y);
            ParseFloat(GetNextToken(LineView), Normal.Z);
            TempNormals.push_back(Normal);
        }
        else if (Header == "mtllib")
        {
            std::string_view MtlFile = TrimView(LineView);

            if (MtlFile.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] mtllib declared without file name. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                continue;
            }
            if (!OutMesh.MaterialLibraryPath.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] Multiple mtllib declarations are not supported. Use first one. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                continue;
            }
            OutMesh.MaterialLibraryPath = FString(MtlFile);
        }
        else if (Header == "usemtl")
        {
            if (!bHasBounds)
    {
        OutMesh.BoundingBox.Min = FVector(0.f, 0.f, 0.f);
        OutMesh.BoundingBox.Max = FVector(0.f, 0.f, 0.f);
    }

    if (bSubMeshActive)
            {
                CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size()) -
                                            CurrentSubMesh.StartIndexLocation;
                if (CurrentSubMesh.IndexCount > 0)
                    OutMesh.SubMeshes.push_back(CurrentSubMesh);
            }

            std::string_view MatName = GetNextToken(LineView);
            CurrentSubMesh.DefaultMaterialName = FString(MatName);
            CurrentSubMesh.StartIndexLocation = static_cast<uint32>(OutMesh.CPU_Indices.size());
            CurrentSubMesh.IndexCount = 0;
            bSubMeshActive = true;
        }
        else if (Header == "f")
        {
            std::vector<uint32> FaceIndices;

            while (true)
            {
                std::string_view VertexToken = GetNextToken(LineView);
                if (VertexToken.empty())
                    break;

                auto It = VertexCache.find(VertexToken);
                if (It != VertexCache.end())
                {
                    FaceIndices.push_back(It->second);
                }
                else
                {
                    int              V = 0, VT = 0, VN = 0;
                    std::string_view Remainder = VertexToken;

                    // "1/2/3" 또는 "1//3" 등 슬래시('/') 기준 파싱
                    auto ParseFaceIndex = [&](int& OutIdx)
                    {
                        size_t           SlashPos = Remainder.find('/');
                        std::string_view SubToken = Remainder.substr(0, SlashPos);
                        ParseInt(SubToken, OutIdx);
                        if (SlashPos != std::string_view::npos)
                            Remainder.remove_prefix(SlashPos + 1);
                        else
                            Remainder = {};
                    };

                    ParseFaceIndex(V);
                    ParseFaceIndex(VT);
                    ParseFaceIndex(VN);

                    FMeshVertexPNCT NewVertex = {};
                    if (V > 0)
                        NewVertex.Position = TempPositions[V - 1];
                    if (VT > 0)
                        NewVertex.UV = TempUVs[VT - 1];
                    if (VN > 0)
                        NewVertex.Normal = TempNormals[VN - 1];

                    uint32 NewIndex = static_cast<uint32>(OutVertices.size());
                    OutVertices.push_back(NewVertex);
                    OutMesh.CPU_Positions.push_back(NewVertex.Position);

                    // VertexToken(string_view) 자체가 Source.FileBytes를 가리키므로 복사 없이 캐시
                    // 키로 사용 가능
                    VertexCache[VertexToken] = NewIndex;
                    FaceIndices.push_back(NewIndex);
                }
            }

            // Triangulation
            for (size_t i = 1; i + 1 < FaceIndices.size(); ++i)
            {
                OutMesh.CPU_Indices.push_back(FaceIndices[0]);
                OutMesh.CPU_Indices.push_back(FaceIndices[i]);
                OutMesh.CPU_Indices.push_back(FaceIndices[i + 1]);
            }
        }
    }

    if (bSubMeshActive)
    {
        CurrentSubMesh.IndexCount =
            static_cast<uint32>(OutMesh.CPU_Indices.size()) - CurrentSubMesh.StartIndexLocation;
        if (CurrentSubMesh.IndexCount > 0)
            OutMesh.SubMeshes.push_back(CurrentSubMesh);
    }
    else if (!OutMesh.CPU_Indices.empty())
    {
        CurrentSubMesh.DefaultMaterialName = "DefaultMaterial";
        CurrentSubMesh.StartIndexLocation = 0;
        CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
        OutMesh.SubMeshes.push_back(CurrentSubMesh);
    }

    OutMesh.VertexCount = static_cast<uint32>(OutVertices.size());
    OutMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
    OutMesh.VertexStride = sizeof(FMeshVertexPNCT);

    return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
}

bool FStaticMeshLoader::CreateBuffers(const TArray<FMeshVertexPNCT>& InVertices,
                                      FStaticMesh&                   OutMesh) const
{
    // RHI (Render Hardware Interface) 유효성 검사
    if (!RHI || !RHI->GetDevice())
    {
        UE_LOG(Asset, ELogVerbosity::Warning, "[StaticMeshLoader] D3D Device is invalid.");
        return false;
    }

    ID3D11Device* Device = RHI->GetDevice();
    HRESULT       Hr = S_OK;

    // ------------------------------------------------------------------
    // 1. Vertex Buffer (VBO) 생성 및 메타데이터 세팅
    // ------------------------------------------------------------------
    if (!InVertices.empty())
    {
        // 렌더링 시 필요한 필수 정보 저장
        OutMesh.VertexCount = static_cast<uint32>(InVertices.size());
        OutMesh.VertexStride = sizeof(FMeshVertexPNCT);

        D3D11_BUFFER_DESC VbDesc = {};
        VbDesc.Usage = D3D11_USAGE_IMMUTABLE; // 데이터가 변하지 않으므로 최적화
        VbDesc.ByteWidth = static_cast<UINT>(OutMesh.VertexStride * OutMesh.VertexCount);
        VbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        VbDesc.CPUAccessFlags = 0;
        VbDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA VbData = {};
        VbData.pSysMem = InVertices.data();

        Hr = Device->CreateBuffer(&VbDesc, &VbData, &OutMesh.VertexBuffer);
        if (FAILED(Hr))
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to create Vertex Buffer. HRESULT=0x%08x",
                   static_cast<uint32>(Hr));
            return false;
        }
    }

    // ------------------------------------------------------------------
    // 2. Index Buffer (IBO) 생성 및 메타데이터 세팅
    // ------------------------------------------------------------------
    if (!OutMesh.CPU_Indices.empty()) // 변경됨: Indices -> CPU_Indices
    {
        // 렌더링 시 필요한 인덱스 개수 저장
        OutMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());

        D3D11_BUFFER_DESC IbDesc = {};
        IbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        IbDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * OutMesh.IndexCount);
        IbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        IbDesc.CPUAccessFlags = 0;
        IbDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA IbData = {};
        IbData.pSysMem = OutMesh.CPU_Indices.data();

        Hr = Device->CreateBuffer(&IbDesc, &IbData, &OutMesh.IndexBuffer);
        if (FAILED(Hr))
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to create Index Buffer. HRESULT=0x%08x",
                   static_cast<uint32>(Hr));
            return false;
        }
    }
    return true;
}

bool FStaticMeshLoader::ParseUAsset(const FSourceRecord& Source, FStaticMesh& OutMesh,
                                   TArray<FMeshVertexPNCT>& OutVertices,
                                   TArray<FString>& OutMaterialAssetPaths) const
{
    FWindowsBinReader Ar(Source.NormalizedPath);
    if (!Ar.IsValid())
    {
        return false;
    }

    FUAssetBinaryHeader Header;
    Header.Magic = 0;
    Header.Version = 0;
    uint32 TypeValue = 0;
    Header.AssetType = EUAssetBinaryType::Unknown;
    Header.SourceHash = 0;

    Ar << Header.Magic;
    Ar << Header.Version;
    Ar << TypeValue;
    Header.AssetType = static_cast<EUAssetBinaryType>(TypeValue);
    Ar << Header.SourceHash;

    if (Header.Magic != kUAssetMagic || Header.Version != kUAssetVersion ||
        Header.AssetType != EUAssetBinaryType::StaticMesh)
    {
        return false;
    }

    Ar << OutMesh.VertexCount;
    Ar << OutMesh.IndexCount;
    Ar << OutMesh.VertexStride;

    Ar.Serialize(&OutMesh.BoundingBox.Min, sizeof(FVector));
    Ar.Serialize(&OutMesh.BoundingBox.Max, sizeof(FVector));

    Ar << OutMesh.MaterialLibraryPath;

    uint32 SubMeshCount = 0;
    Ar << SubMeshCount;
    OutMesh.SubMeshes.clear();
    OutMesh.SubMeshes.reserve(SubMeshCount);
    for (uint32 i = 0; i < SubMeshCount; ++i)
    {
        FSubMesh Sub;
        Ar << Sub.DefaultMaterialName;
        Ar << Sub.StartIndexLocation;
        Ar << Sub.IndexCount;
        OutMesh.SubMeshes.push_back(std::move(Sub));
    }

    uint32 MaterialPathCount = 0;
    Ar << MaterialPathCount;
    OutMaterialAssetPaths.clear();
    OutMaterialAssetPaths.reserve(MaterialPathCount);
    for (uint32 i = 0; i < MaterialPathCount; ++i)
    {
        FString Path;
        Ar << Path;
        OutMaterialAssetPaths.push_back(std::move(Path));
    }

    SerializeArrayRaw(Ar, OutMesh.CPU_Positions);
    SerializeArray(Ar, OutMesh.CPU_Indices);
    SerializeArrayRaw(Ar, OutVertices);

    if (Ar.HasError())
    {
        return false;
    }

    if (!OutMesh.CPU_Positions.empty())
    {
        FVector Min = OutMesh.CPU_Positions[0];
        FVector Max = OutMesh.CPU_Positions[0];
        for (const FVector& Pos : OutMesh.CPU_Positions)
        {
            Min.X = std::min(Min.X, Pos.X);
            Min.Y = std::min(Min.Y, Pos.Y);
            Min.Z = std::min(Min.Z, Pos.Z);

            Max.X = std::max(Max.X, Pos.X);
            Max.Y = std::max(Max.Y, Pos.Y);
            Max.Z = std::max(Max.Z, Pos.Z);
        }
        OutMesh.BoundingBox.Min = Min;
        OutMesh.BoundingBox.Max = Max;
    }

    return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
}

void FStaticMeshLoader::ApplyMaterialAssetPaths(Engine::Asset::UStaticMesh& MeshAsset,
                                                const TArray<FString>& MaterialAssetPaths) const
{
    if (!AssetManager)
    {
        return;
    }

    const uint32 SlotCount = MeshAsset.GetMaterialSlotsSize();
    const uint32 PathCount = static_cast<uint32>(MaterialAssetPaths.size());
    const uint32 Count = (SlotCount < PathCount) ? SlotCount : PathCount;

    for (uint32 i = 0; i < Count; ++i)
    {
        const FString& PathUtf8 = MaterialAssetPaths[i];
        if (PathUtf8.empty())
        {
            continue;
        }

        FWString WidePath = NarrowPathToWidePath(PathUtf8);
        if (WidePath.empty())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to decode material path. RawPath=%s",
                   PathUtf8.c_str());
            continue;
        }

        FAssetLoadParams MatParams;
        MatParams.ExplicitType = EAssetType::Material;

        UAsset* LoadedAsset = AssetManager->Load(WidePath, MatParams);
        if (!LoadedAsset)
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to load material asset. Path=%s",
                   PathUtf8.c_str());
            continue;
        }

        if (auto* MaterialInterface = Cast<Engine::Asset::UMaterialInterface>(LoadedAsset))
        {
            MeshAsset.SetMaterialSlot(i, MaterialInterface);
        }
    }
}
