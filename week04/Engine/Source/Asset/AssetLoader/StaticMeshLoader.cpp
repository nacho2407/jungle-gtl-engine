#include "Core/CoreMinimal.h"
#include "StaticMeshLoader.h"
#include "Asset/StaticMesh.h"
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

    TArray<FMeshVertexNormal> ParsedVertices;

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

    // 3. 에셋 생성 및 리소스 복사 (FontAsset 방식과 동일)
    UStaticMesh* NewMeshAsset = new UStaticMesh();
    NewMeshAsset->Initialize(Source, MeshResource);

    // .obj 파일을 읽으면서 기록해두었던 .mtl 파일들을 load 하도록 AssetManager의 Load 함수 호출
    if (AssetManager) // 포인터가 유효한지 확인
    {
        const fs::path ObjFilePath(Source.NormalizedPath);
        const fs::path ObjDir = ObjFilePath.parent_path();

        for (const FString& MtlFileName : MeshResource->MaterialLibraryPaths)
        {
            const fs::path MtlRelativePath(MtlFileName.c_str());

            // 디렉토리 경로와 파일 이름을 결합 (예: "Data/Models" / "InteriorTest1.mtl")
            const fs::path FullMtlPath = ObjDir / MtlRelativePath;

            // fs::path의 내장 함수를 사용하여 최종 경로를 FWString(std::wstring)으로 변환
            FWString WideMtlPath = FullMtlPath.wstring();

            FAssetLoadParams MatParams;
            MatParams.ExplicitType = EAssetType::Material;


            UAsset* LoadedAsset = AssetManager->Load(WideMtlPath, MatParams);
            if (LoadedAsset)
            {
                UMaterialAsset* MatAsset = static_cast<UMaterialAsset*>(LoadedAsset);

                // 앞서 StaticMesh.h에 추가했던 종속성 배열에 넣어 생명주기를 묶어줍니다.
                // (NewMeshAsset은 현재 Initialize 중인 UStaticMesh 포인터라고 가정합니다)
                NewMeshAsset->AddMaterialDependency(MatAsset);
            }
            else
            {
                // 3. 실패 시 에디터 콘솔이나 로그 창에 명확한 경고를 남깁니다.
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[StaticMeshLoader] Failed to load .mtl file. Path=%s",
                       WidePathToUtf8(WideMtlPath).c_str());
            }
        }
    }
    return NewMeshAsset;
}

bool FStaticMeshLoader::ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
                                     TArray<FMeshVertexNormal>& OutVertices) const
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
            while (true)
            {
                std::string_view MtlFile = GetNextToken(LineView);
                if (MtlFile.empty())
                    break;
                // string_view를 FString으로 변환하여 저장 (단어 단위라 부하 적음)
                OutMesh.MaterialLibraryPaths.push_back(FString(MtlFile));
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

                    FMeshVertexNormal NewVertex = {};
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
    OutMesh.VertexStride = sizeof(FMeshVertexNormal);

    return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
    //if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
    //    return false;

    //FString            FileString(reinterpret_cast<const char*>(Source.FileBytes.data()), Source.FileBytes.size());
    //std::istringstream Stream(FileString);
    //FString            Line;

    //std::vector<FVector>  TempPositions;
    //std::vector<FVector2> TempUVs;
    //std::vector<FVector>  TempNormals;

    //// 중복 정점 방지를 위한 해시 맵 ("v/vt/vn" 문자열 -> 인덱스)
    //TMap<FString, uint32> VertexCache;

    //FSubMesh CurrentSubMesh;
    //bool     bSubMeshActive = false;

    //while (std::getline(Stream, Line))
    //{
    //    if (Line.empty() || Line[0] == '#')
    //        continue;

    //    std::istringstream LineStream(Line);
    //    FString            Header;
    //    LineStream >> Header;

    //    if (Header == "mtllib")
    //    {
    //        FString MtlFileName;
    //        // 한 줄에 여러 개의 .mtl 파일이 선언될 수 있으므로 루프를 돌며 읽습니다.
    //        while (LineStream >> MtlFileName)
    //        {
    //            // 필요하다면 여기서 절대 경로/상대 경로 변환을 수행할 수 있습니다.
    //            OutMesh.MaterialLibraryPaths.push_back(MtlFileName);
    //        }
    //    }
    //    else if (Header == "v")
    //    {
    //        FVector Pos;
    //        LineStream >> Pos.X >> Pos.Y >> Pos.Z;
    //        TempPositions.push_back(Pos);

    //        // AABB 계산 (Bounding Box 갱신)
    //        OutMesh.BoundingBox.Min.X = std::min(OutMesh.BoundingBox.Min.X, Pos.X);
    //        OutMesh.BoundingBox.Min.Y = std::min(OutMesh.BoundingBox.Min.Y, Pos.Y);
    //        OutMesh.BoundingBox.Min.Z = std::min(OutMesh.BoundingBox.Min.Z, Pos.Z);
    //        OutMesh.BoundingBox.Max.X = std::max(OutMesh.BoundingBox.Max.X, Pos.X);
    //        OutMesh.BoundingBox.Max.Y = std::max(OutMesh.BoundingBox.Max.Y, Pos.Y);
    //        OutMesh.BoundingBox.Max.Z = std::max(OutMesh.BoundingBox.Max.Z, Pos.Z);
    //    }
    //    else if (Header == "vt")
    //    {
    //        FVector2 UV;
    //        LineStream >> UV.X >> UV.Y;
    //        UV.Y = 1.0f - UV.Y; // DirectX 환경을 위해 V 좌표 뒤집기 (필요에 따라 제거)
    //        TempUVs.push_back(UV);
    //    }
    //    else if (Header == "vn")
    //    {
    //        FVector Normal;
    //        LineStream >> Normal.X >> Normal.Y >> Normal.Z;
    //        TempNormals.push_back(Normal);
    //    }
    //    else if (Header == "usemtl")
    //    {
    //        if (bSubMeshActive)
    //        {
    //            // 이전 SubMesh 구역 닫기
    //            CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size()) -
    //                                        CurrentSubMesh.StartIndexLocation;
    //            if (CurrentSubMesh.IndexCount > 0)
    //            {
    //                OutMesh.SubMeshes.push_back(CurrentSubMesh);
    //            }
    //        }

    //        LineStream >> CurrentSubMesh.DefaultMaterialName;
    //        CurrentSubMesh.StartIndexLocation = static_cast<uint32>(OutMesh.CPU_Indices.size());
    //        CurrentSubMesh.IndexCount = 0;
    //        bSubMeshActive = true;
    //    }
    //    else if (Header == "f")
    //    {
    //        // FString VertexToken;
    //        // while (LineStream >> VertexToken)
    //        //{
    //        //     auto It = VertexCache.find(VertexToken);
    //        //     if (It != VertexCache.end())
    //        //     {
    //        //         OutMesh.CPU_Indices.push_back(It->second);
    //        //     }
    //        //     else
    //        //     {
    //        //         std::istringstream TokenStream(VertexToken);
    //        //         FString            V_Str, VT_Str, VN_Str;

    //        //        std::getline(TokenStream, V_Str, '/');
    //        //        std::getline(TokenStream, VT_Str, '/');
    //        //        std::getline(TokenStream, VN_Str, '/');

    //        //        FMeshVertexNormal NewVertex = {};
    //        //        // OBJ 인덱스는 1부터 시작하므로 1을 빼줍니다.
    //        //        if (!V_Str.empty())
    //        //            NewVertex.Position = TempPositions[std::stoi(V_Str) - 1];
    //        //        if (!VT_Str.empty())
    //        //            NewVertex.UV = TempUVs[std::stoi(VT_Str) - 1];
    //        //        if (!VN_Str.empty())
    //        //            NewVertex.Normal = TempNormals[std::stoi(VN_Str) - 1];

    //        //        uint32 NewIndex = static_cast<uint32>(OutVertices.size());
    //        //        OutVertices.push_back(NewVertex);
    //        //        OutMesh.CPU_Indices.push_back(NewIndex);

    //        //        // CPU 충돌/Picking용 위치 데이터 복사본 유지
    //        //        OutMesh.CPU_Positions.push_back(NewVertex.Position);

    //        //        VertexCache[VertexToken] = NewIndex;
    //        //    }
    //        //}

    //        FString VertexToken;
    //        std::vector<uint32>
    //            FaceIndices; // 현재 다각형(Face)을 구성하는 정점 인덱스들을 임시 저장

    //        while (LineStream >> VertexToken)
    //        {
    //            auto It = VertexCache.find(VertexToken);
    //            if (It != VertexCache.end())
    //            {
    //                FaceIndices.push_back(It->second);
    //            }
    //            else
    //            {
    //                std::istringstream TokenStream(VertexToken);
    //                FString            V_Str, VT_Str, VN_Str;

    //                std::getline(TokenStream, V_Str, '/');
    //                std::getline(TokenStream, VT_Str, '/');
    //                std::getline(TokenStream, VN_Str, '/');

    //                FMeshVertexNormal NewVertex = {};
    //                if (!V_Str.empty())
    //                    NewVertex.Position = TempPositions[std::stoi(V_Str) - 1];
    //                if (!VT_Str.empty())
    //                    NewVertex.UV = TempUVs[std::stoi(VT_Str) - 1];
    //                if (!VN_Str.empty())
    //                    NewVertex.Normal = TempNormals[std::stoi(VN_Str) - 1];

    //                uint32 NewIndex = static_cast<uint32>(OutVertices.size());
    //                OutVertices.push_back(NewVertex);

    //                // CPU 충돌용 위치 데이터
    //                OutMesh.CPU_Positions.push_back(NewVertex.Position);

    //                VertexCache[VertexToken] = NewIndex;
    //                FaceIndices.push_back(NewIndex);
    //            }
    //        }

    //        // --- Triangulation (다각형을 삼각형으로 분할) ---
    //        // 점이 3개면 1바퀴, 4개면 2바퀴 돌면서 삼각형 생성
    //        for (size_t i = 1; i + 1 < FaceIndices.size(); ++i)
    //        {
    //            OutMesh.CPU_Indices.push_back(FaceIndices[0]);     // 기준점 (v0)
    //            OutMesh.CPU_Indices.push_back(FaceIndices[i]);     // v1, v2 ...
    //            OutMesh.CPU_Indices.push_back(FaceIndices[i + 1]); // v2, v3 ...
    //        }
    //    }
    //}

    //// 파일 끝에 도달했을 때 마지막 SubMesh 닫기
    //if (bSubMeshActive)
    //{
    //    CurrentSubMesh.IndexCount =
    //        static_cast<uint32>(OutMesh.CPU_Indices.size()) - CurrentSubMesh.StartIndexLocation;
    //    if (CurrentSubMesh.IndexCount > 0)
    //    {
    //        OutMesh.SubMeshes.push_back(CurrentSubMesh);
    //    }
    //}
    //// 머티리얼 정보가 전혀 없는 obj 파일에 대한 예외 처리
    //else if (!OutMesh.CPU_Indices.empty())
    //{
    //    CurrentSubMesh.DefaultMaterialName = "DefaultMaterial";
    //    CurrentSubMesh.StartIndexLocation = 0;
    //    CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
    //    OutMesh.SubMeshes.push_back(CurrentSubMesh);
    //}

    //OutMesh.VertexCount = static_cast<uint32>(OutVertices.size());
    //OutMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
    //OutMesh.VertexStride = sizeof(FMeshVertexNormal);

    //return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
}

bool FStaticMeshLoader::CreateBuffers(const TArray<FMeshVertexNormal>& InVertices,
                                      FStaticMeshResource&             OutMesh) const
{
    if (!RHI || !RHI->GetDevice())
        return false;

    // 1. 버텍스 버퍼 생성
    D3D11_BUFFER_DESC VbDesc = {};
    VbDesc.ByteWidth = sizeof(FMeshVertexNormal) * OutMesh.VertexCount;
    VbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VbData = {};
    VbData.pSysMem = InVertices.data();

    HRESULT Hr = RHI->GetDevice()->CreateBuffer(&VbDesc, &VbData, &OutMesh.VertexBuffer);
    if (FAILED(Hr))
        return false;

    // 2. 인덱스 버퍼 생성
    D3D11_BUFFER_DESC IbDesc = {};
    IbDesc.ByteWidth = sizeof(uint32) * OutMesh.IndexCount;
    IbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IbData = {};
    IbData.pSysMem = OutMesh.CPU_Indices.data();

    Hr = RHI->GetDevice()->CreateBuffer(&IbDesc, &IbData, &OutMesh.IndexBuffer);
    if (FAILED(Hr))
        return false;

    return true;
}

FString FStaticMeshLoader::WidePathToUtf8(const FWString& Path) const
{
    const fs::path      FilePath(Path);
    const std::u8string Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}