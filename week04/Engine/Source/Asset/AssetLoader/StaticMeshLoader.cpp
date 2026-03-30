#include "Core/CoreMinimal.h"
#include "StaticMeshLoader.h"
#include "Asset/StaticMesh.h"
#include "Asset/Material.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/Types/VertexTypes.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>
#include <string_view>

namespace fs = std::filesystem;

FStaticMeshLoader::FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager)
    : RHI(InRHI), AssetManager(InAssetManager)
{
}

bool FStaticMeshLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Path.empty())
        return false;

    if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::StaticMesh)
    {
        return false;
    }

    const fs::path FilePath(Path);
    FWString       Extension = FilePath.extension().native();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });

    return Extension == L".obj";
}

EAssetType FStaticMeshLoader::GetAssetType() const { return EAssetType::StaticMesh; }

uint64 FStaticMeshLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
    (void)Params;
    uint64 Hash = 14695981039346656037ull;
    Hash ^= static_cast<uint64>(GetAssetType());
    Hash *= 1099511628211ull;
    return Hash;
}

UAsset* FStaticMeshLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    std::shared_ptr<FStaticMeshResource> MeshResource = std::make_shared<FStaticMeshResource>();
    MeshResource->Reset();

    TArray<FMeshVertexPNCT> ParsedVertices;

    // 1. OBJ 텍스트 파싱 및 CPU 데이터 구성
    if (!ParseObjText(Source, *MeshResource, ParsedVertices))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[StaticMeshLoader] Failed to parse .obj file. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return nullptr;
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

    // --- 추가: 서브 메시 개수에 맞춰 머티리얼 슬롯 미리 확보 ---
    const uint32 NumSubMeshes = static_cast<uint32>(MeshResource->SubMeshes.size());
    NewMeshAsset->InitializeMaterialSlots(NumSubMeshes);

    // .mtl 파일 로드 및 매핑
    if (AssetManager && !MeshResource->MaterialLibraryPath.empty())
    {
        const fs::path ObjFilePath(Source.NormalizedPath);
        const fs::path ObjDir = ObjFilePath.parent_path();

        const fs::path MtlRelativePath(MeshResource->MaterialLibraryPath.c_str());
        const fs::path FullMtlPath = ObjDir / MtlRelativePath;
        FWString       WideMtlPath = FullMtlPath.wstring();

        FAssetLoadParams MatParams;
        MatParams.ExplicitType = EAssetType::Material;

        UAsset* LoadedAsset = AssetManager->Load(WideMtlPath, MatParams);
        Engine::Asset::UMaterial* LoadedMtl = static_cast<Engine::Asset::UMaterial*>(LoadedAsset);

        if (!LoadedMtl)        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshLoader] Failed to load .mtl file. Path=%s",
                   WidePathToUtf8(WideMtlPath).c_str());
            return NewMeshAsset;
        }

        for (uint32 i = 0; i < NumSubMeshes; ++i)
        {
            const FString& TargetMatName = MeshResource->SubMeshes[i].DefaultMaterialName;

            if (LoadedMtl->GetMaterialData(TargetMatName) != nullptr)
            {
                NewMeshAsset->SetMaterialSlot(i, LoadedMtl, TargetMatName);
            }
            else
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] Material '%s' not found in .mtl '%s'",
                       TargetMatName.c_str(), WidePathToUtf8(WideMtlPath).c_str());
                // 추후 기본 머터리얼 asset으로 지정 필요
                NewMeshAsset->SetMaterialSlot(i, nullptr, "");
            }
        }
    }
    return NewMeshAsset;
}

bool FStaticMeshLoader::ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
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

    // --- 메인 파싱 루프 ---
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

            OutMesh.BoundingBox.Min.X = std::min(OutMesh.BoundingBox.Min.X, Pos.X);
            OutMesh.BoundingBox.Min.Y = std::min(OutMesh.BoundingBox.Min.Y, Pos.Y);
            OutMesh.BoundingBox.Min.Z = std::min(OutMesh.BoundingBox.Min.Z, Pos.Z);
            OutMesh.BoundingBox.Max.X = std::max(OutMesh.BoundingBox.Max.X, Pos.X);
            OutMesh.BoundingBox.Max.Y = std::max(OutMesh.BoundingBox.Max.Y, Pos.Y);
            OutMesh.BoundingBox.Max.Z = std::max(OutMesh.BoundingBox.Max.Z, Pos.Z);
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
            std::string_view MtlFile = GetNextToken(LineView);
            if (MtlFile.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] mtllib declared without file name. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                return false;
            }
            if (!OutMesh.MaterialLibraryPath.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] Multiple mtllib declarations are not supported. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                return false;
            }

            OutMesh.MaterialLibraryPath = FString(MtlFile);

            // 같은 줄에 두 번째 파일명이 더 있으면 실패
            if (!GetNextToken(LineView).empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] Multiple mtllib entries in one declaration are not "
                       "supported. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                return false;
            }
        }
        else if (Header == "usemtl")
        {
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
                                      FStaticMeshResource&             OutMesh) const
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

FString FStaticMeshLoader::WidePathToUtf8(const FWString& Path) const
{
    const fs::path      FilePath(Path);
    const std::u8string Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}
