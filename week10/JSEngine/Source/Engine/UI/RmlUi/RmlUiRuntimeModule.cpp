#include "UI/RmlUi/RmlUiRuntimeModule.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/CallbackTexture.h"
#include "RmlUi/Core/FileInterface.h"
#include "RmlUi/Core/FontEngineInterface.h"
#include "RmlUi/Core/Mesh.h"
#include "RmlUi/Core/Log.h"
#include "RmlUi/Core/RenderManager.h"
#include "RmlUi/Core/SystemInterface.h"

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
    class FJSRmlUiSystemInterface final : public Rml::SystemInterface
    {
    public:
        double GetElapsedTime() override
        {
            using FClock = std::chrono::steady_clock;
            const auto Now = FClock::now();
            return std::chrono::duration<double>(Now - StartTime).count();
        }

        bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override
        {
            switch (Type)
            {
            case Rml::Log::LT_ERROR:
            case Rml::Log::LT_ASSERT:
                UE_LOG_ERROR("[RmlUi] %s", Message.c_str());
                break;
            case Rml::Log::LT_WARNING:
                UE_LOG_WARNING("[RmlUi] %s", Message.c_str());
                break;
            default:
                UE_LOG("[RmlUi] %s", Message.c_str());
                break;
            }
            return true;
        }

    private:
        std::chrono::steady_clock::time_point StartTime = std::chrono::steady_clock::now();
    };

    class FJSRmlUiFileInterface final : public Rml::FileInterface
    {
    public:
        Rml::FileHandle Open(const Rml::String& Path) override
        {
            std::filesystem::path ResolvedPath = ResolvePath(Path);
            auto* Stream = new std::ifstream(ResolvedPath, std::ios::binary);
            if (!Stream->is_open())
            {
                UE_LOG_WARNING("[RmlUi] Failed to open file: %s", FPaths::ToUtf8(ResolvedPath.generic_wstring()).c_str());
                delete Stream;
                return 0;
            }

            return reinterpret_cast<Rml::FileHandle>(Stream);
        }

        void Close(Rml::FileHandle File) override
        {
            auto* Stream = reinterpret_cast<std::ifstream*>(File);
            delete Stream;
        }

        size_t Read(void* Buffer, size_t Size, Rml::FileHandle File) override
        {
            auto* Stream = reinterpret_cast<std::ifstream*>(File);
            if (!Stream || !Stream->good())
            {
                return 0;
            }

            Stream->read(static_cast<char*>(Buffer), static_cast<std::streamsize>(Size));
            return static_cast<size_t>(Stream->gcount());
        }

        bool Seek(Rml::FileHandle File, long Offset, int Origin) override
        {
            auto* Stream = reinterpret_cast<std::ifstream*>(File);
            if (!Stream)
            {
                return false;
            }

            std::ios_base::seekdir Direction = std::ios::beg;
            if (Origin == SEEK_CUR)
            {
                Direction = std::ios::cur;
            }
            else if (Origin == SEEK_END)
            {
                Direction = std::ios::end;
            }

            Stream->clear();
            Stream->seekg(Offset, Direction);
            return !Stream->fail();
        }

        size_t Tell(Rml::FileHandle File) override
        {
            auto* Stream = reinterpret_cast<std::ifstream*>(File);
            if (!Stream)
            {
                return 0;
            }

            const std::streampos Position = Stream->tellg();
            if (Position < 0)
            {
                return 0;
            }
            return static_cast<size_t>(Position);
        }

    private:
        std::filesystem::path ResolvePath(const Rml::String& Path) const
        {
            std::filesystem::path Candidate(FPaths::ToWide(FPaths::Normalize(Path)));
            if (Candidate.is_absolute())
            {
                return Candidate.lexically_normal();
            }

            return (std::filesystem::path(FPaths::RootDir()) / Candidate).lexically_normal();
        }
    };

    class FJSRmlUiGdiFontEngine final : public Rml::FontEngineInterface
    {
        struct FFontFaceInfo
        {
            Rml::String Family;
            int Size = 16;
            Rml::Style::FontWeight Weight = Rml::Style::FontWeight::Normal;
        };

    public:
        ~FJSRmlUiGdiFontEngine()
        {
            ReleaseRegisteredFontFiles();
        }

        void ReleaseCachedRenderResources()
        {
            GeneratedTextures.clear();
            ++Version;
        }

        bool LoadFontFace(const Rml::String& FileName, int FaceIndex, bool bFallbackFace, Rml::Style::FontWeight Weight) override
        {
            (void)FaceIndex;
            (void)bFallbackFace;
            (void)Weight;
            return RegisterFontFile(FileName);
        }

        bool LoadFontFace(
            const Rml::String& FileName,
            int FaceIndex,
            const Rml::String& Family,
            Rml::Style::FontStyle Style,
            Rml::Style::FontWeight Weight,
            bool bFallbackFace) override
        {
            (void)FaceIndex;
            (void)Style;
            (void)Weight;
            (void)bFallbackFace;
            const bool bLoaded = RegisterFontFile(FileName);
            if (bLoaded && !Family.empty())
            {
                const Rml::String ActualFamily = ExtractFontFamilyName(ResolveFontPath(FileName));
                if (!ActualFamily.empty())
                {
                    FontFamilyAliases[MakeFontAliasKey(Family, Weight)] = ActualFamily;
                    if (Weight == Rml::Style::FontWeight::Normal || Weight == Rml::Style::FontWeight::Auto)
                    {
                        FontFamilyAliases[Family] = ActualFamily;
                    }
                    UE_LOG("[RmlUi] Font family alias: %s -> %s", Family.c_str(), ActualFamily.c_str());
                }
            }
            UE_LOG("[RmlUi] Registered GDI font face request: %s Family=%s", FileName.c_str(), Family.c_str());
            return bLoaded;
        }

        bool LoadFontFace(
            Rml::Span<const Rml::byte> Data,
            int FaceIndex,
            const Rml::String& Family,
            Rml::Style::FontStyle Style,
            Rml::Style::FontWeight Weight,
            bool bFallbackFace) override
        {
            (void)Data;
            (void)FaceIndex;
            (void)Style;
            (void)Weight;
            (void)bFallbackFace;
            return true;
        }

        Rml::FontFaceHandle GetFontFaceHandle(
            const Rml::String& Family,
            Rml::Style::FontStyle Style,
            Rml::Style::FontWeight Weight,
            int Size) override
        {
            (void)Style;
            (void)Weight;

            const int ClampedSize = std::max(Size, 8);
            EnsureMetrics(ClampedSize);
            const int Handle = NextHandle++;
            FaceByHandle[Handle] = FFontFaceInfo{
                ResolveFamilyName(Family, Weight),
                ClampedSize,
                Weight
            };
            return static_cast<Rml::FontFaceHandle>(Handle);
        }

        Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle Handle, const Rml::FontEffectList& FontEffects) override
        {
            (void)Handle;
            (void)FontEffects;
            return 1;
        }

        const Rml::FontMetrics& GetFontMetrics(Rml::FontFaceHandle Handle) override
        {
            return EnsureMetrics(HandleToSize(Handle));
        }

        int GetStringWidth(
            Rml::FontFaceHandle Handle,
            Rml::StringView String,
            const Rml::TextShapingContext& TextShapingContext,
            Rml::Character PriorCharacter = Rml::Character::Null) override
        {
            (void)TextShapingContext;
            (void)PriorCharacter;

            const std::wstring Wide = Utf8ToWide(String);
            if (Wide.empty())
            {
                return 0;
            }

            const FFontFaceInfo Face = GetFaceInfo(Handle);
            HFONT Font = CreateGdiFont(Face.Size, Face.Family, Face.Weight);
            HDC DC = ::CreateCompatibleDC(nullptr);
            HGDIOBJ OldFont = ::SelectObject(DC, Font);
            SIZE TextSize = {};
            ::GetTextExtentPoint32W(DC, Wide.c_str(), static_cast<int>(Wide.size()), &TextSize);
            ::SelectObject(DC, OldFont);
            ::DeleteObject(Font);
            ::DeleteDC(DC);
            return std::max(static_cast<int>(TextSize.cx), 0);
        }

        int GenerateString(
            Rml::RenderManager& RenderManager,
            Rml::FontFaceHandle FaceHandle,
            Rml::FontEffectsHandle FontEffectsHandle,
            Rml::StringView String,
            Rml::Vector2f Position,
            Rml::ColourbPremultiplied Colour,
            float Opacity,
            const Rml::TextShapingContext& TextShapingContext,
            Rml::TexturedMeshList& MeshList) override
        {
            (void)FontEffectsHandle;
            (void)TextShapingContext;

            const std::wstring Wide = Utf8ToWide(String);
            if (Wide.empty())
            {
                return 0;
            }

            const FFontFaceInfo Face = GetFaceInfo(FaceHandle);
            const Rml::FontMetrics& Metrics = EnsureMetrics(Face.Size);
            const int AdvanceWidth = std::max(GetStringWidth(FaceHandle, String, TextShapingContext), 1);
            const int PaddingX = std::max(4, Face.Size / 4);
            const int PaddingY = std::max(2, Face.Size / 8);
            const int Width = AdvanceWidth + PaddingX * 2;
            const int Height = std::max(static_cast<int>(Metrics.line_spacing + 2.0f), Face.Size + 4) + PaddingY * 2;
            std::vector<Rml::byte> Pixels(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4, 0);

            RenderTextToPixels(Wide, Face.Size, Face.Family, Face.Weight, Width, Height, PaddingX, PaddingY, Colour, Opacity, Pixels);

            const Rml::Vector2i Dimensions(Width, Height);
            Rml::CallbackTexture TextureResource = RenderManager.MakeCallbackTexture(
                [Pixels = std::move(Pixels), Dimensions](const Rml::CallbackTextureInterface& TextureInterface)
                {
                    return TextureInterface.GenerateTexture(Rml::Span<const Rml::byte>(Pixels.data(), Pixels.size()), Dimensions);
                });
            Rml::Texture Texture = TextureResource;
            GeneratedTextures.push_back(std::move(TextureResource));

            Rml::Mesh Mesh;
            const float Left = Position.x - static_cast<float>(PaddingX);
            const float Top = Position.y - Metrics.ascent - static_cast<float>(PaddingY);
            const float Right = Left + static_cast<float>(Width);
            const float Bottom = Top + static_cast<float>(Height);
            const Rml::ColourbPremultiplied White(255, 255, 255, 255);
            Mesh.vertices = {
                { Rml::Vector2f(Left, Top), White, Rml::Vector2f(0.0f, 0.0f) },
                { Rml::Vector2f(Right, Top), White, Rml::Vector2f(1.0f, 0.0f) },
                { Rml::Vector2f(Right, Bottom), White, Rml::Vector2f(1.0f, 1.0f) },
                { Rml::Vector2f(Left, Bottom), White, Rml::Vector2f(0.0f, 1.0f) },
            };
            Mesh.indices = { 0, 1, 2, 0, 2, 3 };
            MeshList.push_back(Rml::TexturedMesh{ std::move(Mesh), Texture });
            return AdvanceWidth;
        }

        int GetVersion(Rml::FontFaceHandle Handle) override
        {
            (void)Handle;
            return Version;
        }

        void ReleaseFontResources() override
        {
            ReleaseCachedRenderResources();
        }

    private:
        int HandleToSize(Rml::FontFaceHandle Handle) const
        {
            return GetFaceInfo(Handle).Size;
        }

        FFontFaceInfo GetFaceInfo(Rml::FontFaceHandle Handle) const
        {
            const int Key = static_cast<int>(Handle);
            auto It = FaceByHandle.find(Key);
            if (It != FaceByHandle.end())
            {
                return It->second;
            }
            return FFontFaceInfo{ "Malgun Gothic", std::max(Key, 8), Rml::Style::FontWeight::Normal };
        }

        Rml::String ResolveFamilyName(const Rml::String& Family, Rml::Style::FontWeight Weight = Rml::Style::FontWeight::Normal) const
        {
            if (Family.empty())
            {
                return "Malgun Gothic";
            }

            auto It = FontFamilyAliases.find(MakeFontAliasKey(Family, Weight));
            if (It != FontFamilyAliases.end() && !It->second.empty())
            {
                return It->second;
            }

            It = FontFamilyAliases.find(Family);
            if (It != FontFamilyAliases.end() && !It->second.empty())
            {
                return It->second;
            }
            return Family;
        }

        Rml::String MakeFontAliasKey(const Rml::String& Family, Rml::Style::FontWeight Weight) const
        {
            return Family + "#" + std::to_string(static_cast<int>(Weight));
        }

        const Rml::FontMetrics& EnsureMetrics(int Size)
        {
            auto It = MetricsBySize.find(Size);
            if (It != MetricsBySize.end())
            {
                return It->second;
            }

            Rml::FontMetrics Metrics = {};
            Metrics.size = Size;
            Metrics.ascent = Size * 0.82f;
            Metrics.descent = Size * 0.22f;
            Metrics.line_spacing = Size * 1.18f;
            Metrics.x_height = Size * 0.52f;
            Metrics.underline_position = Size * 0.10f;
            Metrics.underline_thickness = std::max(1.0f, Size * 0.06f);
            Metrics.has_ellipsis = true;
            return MetricsBySize.emplace(Size, Metrics).first->second;
        }

        HFONT CreateGdiFont(int Size, const Rml::String& FamilyName) const
        {
            return CreateGdiFont(Size, FamilyName, Rml::Style::FontWeight::Normal);
        }

        HFONT CreateGdiFont(int Size, const Rml::String& FamilyName, Rml::Style::FontWeight Weight) const
        {
            const std::wstring Family = Utf8ToWide(FamilyName.empty() ? Rml::String("Malgun Gothic") : FamilyName);
            const int GdiWeight = static_cast<int>(Weight) >= static_cast<int>(Rml::Style::FontWeight::Bold)
                ? FW_BOLD
                : FW_NORMAL;
            return ::CreateFontW(
                -Size,
                0,
                0,
                0,
                GdiWeight,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                Family.c_str());
        }

        std::wstring Utf8ToWide(Rml::StringView String) const
        {
            if (String.empty())
            {
                return {};
            }

            const int SourceLength = static_cast<int>(String.size());
            int WideLength = ::MultiByteToWideChar(CP_UTF8, 0, String.begin(), SourceLength, nullptr, 0);
            if (WideLength <= 0)
            {
                return {};
            }

            std::wstring Wide(static_cast<size_t>(WideLength), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, String.begin(), SourceLength, Wide.data(), WideLength);
            return Wide;
        }

        Rml::String WideToUtf8(const std::wstring& String) const
        {
            if (String.empty())
            {
                return {};
            }

            const int SourceLength = static_cast<int>(String.size());
            int Utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, String.c_str(), SourceLength, nullptr, 0, nullptr, nullptr);
            if (Utf8Length <= 0)
            {
                return {};
            }

            Rml::String Utf8(static_cast<size_t>(Utf8Length), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, String.c_str(), SourceLength, Utf8.data(), Utf8Length, nullptr, nullptr);
            return Utf8;
        }

        void RenderTextToPixels(
            const std::wstring& Text,
            int FontSize,
            const Rml::String& FamilyName,
            Rml::Style::FontWeight Weight,
            int Width,
            int Height,
            int TextOffsetX,
            int TextOffsetY,
            Rml::ColourbPremultiplied Colour,
            float Opacity,
            std::vector<Rml::byte>& Pixels) const
        {
            BITMAPINFO BitmapInfo = {};
            BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            BitmapInfo.bmiHeader.biWidth = Width;
            BitmapInfo.bmiHeader.biHeight = -Height;
            BitmapInfo.bmiHeader.biPlanes = 1;
            BitmapInfo.bmiHeader.biBitCount = 32;
            BitmapInfo.bmiHeader.biCompression = BI_RGB;

            void* Bits = nullptr;
            HDC DC = ::CreateCompatibleDC(nullptr);
            HBITMAP Bitmap = ::CreateDIBSection(DC, &BitmapInfo, DIB_RGB_COLORS, &Bits, nullptr, 0);
            if (!Bitmap || !Bits)
            {
                if (Bitmap)
                {
                    ::DeleteObject(Bitmap);
                }
                ::DeleteDC(DC);
                return;
            }

            HGDIOBJ OldBitmap = ::SelectObject(DC, Bitmap);
            HFONT Font = CreateGdiFont(FontSize, FamilyName, Weight);
            HGDIOBJ OldFont = ::SelectObject(DC, Font);
            RECT Rect{ TextOffsetX, TextOffsetY, Width, Height };
            HBRUSH BlackBrush = ::CreateSolidBrush(RGB(0, 0, 0));
            ::FillRect(DC, &Rect, BlackBrush);
            ::DeleteObject(BlackBrush);
            ::SetBkMode(DC, TRANSPARENT);
            ::SetTextColor(DC, RGB(255, 255, 255));
            ::DrawTextW(DC, Text.c_str(), static_cast<int>(Text.size()), &Rect, DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE);

            const Rml::byte* Source = static_cast<const Rml::byte*>(Bits);
            const float OpacityScale = std::clamp(Opacity, 0.0f, 1.0f);
            const float ColourAlpha = Colour.alpha / 255.0f;
            const Rml::byte StraightRed = ColourAlpha > 0.0f ? static_cast<Rml::byte>(std::clamp(Colour.red / ColourAlpha, 0.0f, 255.0f)) : Colour.red;
            const Rml::byte StraightGreen = ColourAlpha > 0.0f ? static_cast<Rml::byte>(std::clamp(Colour.green / ColourAlpha, 0.0f, 255.0f)) : Colour.green;
            const Rml::byte StraightBlue = ColourAlpha > 0.0f ? static_cast<Rml::byte>(std::clamp(Colour.blue / ColourAlpha, 0.0f, 255.0f)) : Colour.blue;
            for (int Y = 0; Y < Height; ++Y)
            {
                for (int X = 0; X < Width; ++X)
                {
                    const size_t Index = (static_cast<size_t>(Y) * Width + X) * 4;
                    const Rml::byte Coverage = std::max(Source[Index + 0], std::max(Source[Index + 1], Source[Index + 2]));
                    const float AlphaScale = (Coverage / 255.0f) * OpacityScale;
                    Pixels[Index + 0] = StraightRed;
                    Pixels[Index + 1] = StraightGreen;
                    Pixels[Index + 2] = StraightBlue;
                    Pixels[Index + 3] = static_cast<Rml::byte>(Colour.alpha * AlphaScale);
                }
            }

            ::SelectObject(DC, OldFont);
            ::SelectObject(DC, OldBitmap);
            ::DeleteObject(Font);
            ::DeleteObject(Bitmap);
            ::DeleteDC(DC);
        }

        std::filesystem::path ResolveFontPath(const Rml::String& FileName) const
        {
            std::filesystem::path Candidate(FPaths::ToWide(FPaths::Normalize(CleanFontFileName(FileName))));
            if (Candidate.is_absolute())
            {
                return Candidate.lexically_normal();
            }
            return (std::filesystem::path(FPaths::RootDir()) / Candidate).lexically_normal();
        }

        Rml::String CleanFontFileName(const Rml::String& FileName) const
        {
            Rml::String Clean = TrimAscii(FileName);
            if (Clean.size() >= 5 && Clean.substr(0, 4) == "url(" && Clean.back() == ')')
            {
                Clean = TrimAscii(Clean.substr(4, Clean.size() - 5));
            }
            if (Clean.size() >= 2
                && ((Clean.front() == '"' && Clean.back() == '"') || (Clean.front() == '\'' && Clean.back() == '\'')))
            {
                Clean = Clean.substr(1, Clean.size() - 2);
            }
            return Clean;
        }

        Rml::String TrimAscii(const Rml::String& Value) const
        {
            size_t Begin = 0;
            while (Begin < Value.size() && std::isspace(static_cast<unsigned char>(Value[Begin])))
            {
                ++Begin;
            }

            size_t End = Value.size();
            while (End > Begin && std::isspace(static_cast<unsigned char>(Value[End - 1])))
            {
                --End;
            }
            return Value.substr(Begin, End - Begin);
        }

        bool RegisterFontFile(const Rml::String& FileName)
        {
            if (FileName.empty())
            {
                return false;
            }

            const std::filesystem::path FontPath = ResolveFontPath(FileName);
            if (!std::filesystem::exists(FontPath))
            {
                UE_LOG_WARNING("[RmlUi] Font file missing: %s -> %s",
                    FileName.c_str(),
                    FPaths::ToUtf8(FontPath.generic_wstring()).c_str());
                return false;
            }

            for (const std::filesystem::path& RegisteredPath : RegisteredFontPaths)
            {
                if (RegisteredPath == FontPath)
                {
                    return true;
                }
            }

            const int LoadedCount = ::AddFontResourceExW(FontPath.c_str(), FR_PRIVATE, nullptr);
            if (LoadedCount <= 0)
            {
                UE_LOG_WARNING("[RmlUi] Failed to load font file: %s", FileName.c_str());
                return false;
            }

            RegisteredFontPaths.push_back(FontPath);
            UE_LOG("[RmlUi] Loaded font file: %s", FileName.c_str());
            return true;
        }

        static std::uint16_t ReadBE16(const std::vector<unsigned char>& Data, size_t Offset)
        {
            if (Offset + 1 >= Data.size())
            {
                return 0;
            }
            return static_cast<std::uint16_t>((Data[Offset] << 8) | Data[Offset + 1]);
        }

        static std::uint32_t ReadBE32(const std::vector<unsigned char>& Data, size_t Offset)
        {
            if (Offset + 3 >= Data.size())
            {
                return 0;
            }
            return (static_cast<std::uint32_t>(Data[Offset]) << 24)
                | (static_cast<std::uint32_t>(Data[Offset + 1]) << 16)
                | (static_cast<std::uint32_t>(Data[Offset + 2]) << 8)
                | static_cast<std::uint32_t>(Data[Offset + 3]);
        }

        Rml::String DecodeFontNameString(
            const std::vector<unsigned char>& Data,
            size_t Offset,
            size_t Length,
            std::uint16_t PlatformId) const
        {
            if (Offset + Length > Data.size() || Length == 0)
            {
                return {};
            }

            if (PlatformId == 0 || PlatformId == 3)
            {
                std::wstring Wide;
                Wide.reserve(Length / 2);
                for (size_t Index = 0; Index + 1 < Length; Index += 2)
                {
                    Wide.push_back(static_cast<wchar_t>(ReadBE16(Data, Offset + Index)));
                }
                return WideToUtf8(Wide);
            }

            Rml::String Result;
            Result.reserve(Length);
            for (size_t Index = 0; Index < Length; ++Index)
            {
                const unsigned char Ch = Data[Offset + Index];
                if (Ch >= 32 && Ch < 127)
                {
                    Result.push_back(static_cast<char>(Ch));
                }
            }
            return Result;
        }

        Rml::String ExtractFontFamilyName(const std::filesystem::path& FontPath) const
        {
            std::ifstream File(FontPath, std::ios::binary | std::ios::ate);
            if (!File.is_open())
            {
                return {};
            }

            const std::streamsize Size = File.tellg();
            if (Size <= 0)
            {
                return {};
            }

            std::vector<unsigned char> Data(static_cast<size_t>(Size));
            File.seekg(0, std::ios::beg);
            if (!File.read(reinterpret_cast<char*>(Data.data()), Size) || Data.size() < 12)
            {
                return {};
            }

            const std::uint16_t TableCount = ReadBE16(Data, 4);
            size_t NameOffset = 0;
            size_t NameLength = 0;
            for (std::uint16_t TableIndex = 0; TableIndex < TableCount; ++TableIndex)
            {
                const size_t RecordOffset = 12 + static_cast<size_t>(TableIndex) * 16;
                if (RecordOffset + 15 >= Data.size())
                {
                    break;
                }

                if (Data[RecordOffset] == 'n'
                    && Data[RecordOffset + 1] == 'a'
                    && Data[RecordOffset + 2] == 'm'
                    && Data[RecordOffset + 3] == 'e')
                {
                    NameOffset = ReadBE32(Data, RecordOffset + 8);
                    NameLength = ReadBE32(Data, RecordOffset + 12);
                    break;
                }
            }

            if (NameOffset == 0 || NameOffset + NameLength > Data.size() || NameLength < 6)
            {
                return {};
            }

            const std::uint16_t NameCount = ReadBE16(Data, NameOffset + 2);
            const size_t StringBase = NameOffset + ReadBE16(Data, NameOffset + 4);
            Rml::String FallbackFamily;
            for (std::uint16_t NameIndex = 0; NameIndex < NameCount; ++NameIndex)
            {
                const size_t RecordOffset = NameOffset + 6 + static_cast<size_t>(NameIndex) * 12;
                if (RecordOffset + 11 >= Data.size())
                {
                    break;
                }

                const std::uint16_t PlatformId = ReadBE16(Data, RecordOffset);
                const std::uint16_t LanguageId = ReadBE16(Data, RecordOffset + 4);
                const std::uint16_t NameId = ReadBE16(Data, RecordOffset + 6);
                if (NameId != 1)
                {
                    continue;
                }

                const size_t TextLength = ReadBE16(Data, RecordOffset + 8);
                const size_t TextOffset = StringBase + ReadBE16(Data, RecordOffset + 10);
                Rml::String Family = DecodeFontNameString(Data, TextOffset, TextLength, PlatformId);
                if (Family.empty())
                {
                    continue;
                }

                if (FallbackFamily.empty())
                {
                    FallbackFamily = Family;
                }
                if (PlatformId == 3 && LanguageId == 0x0409)
                {
                    return Family;
                }
            }

            return FallbackFamily;
        }

        void ReleaseRegisteredFontFiles()
        {
            for (const std::filesystem::path& FontPath : RegisteredFontPaths)
            {
                ::RemoveFontResourceExW(FontPath.c_str(), FR_PRIVATE, nullptr);
            }
            RegisteredFontPaths.clear();
        }

        int NextHandle = 1;
        std::unordered_map<int, Rml::FontMetrics> MetricsBySize;
        std::unordered_map<int, FFontFaceInfo> FaceByHandle;
        std::unordered_map<Rml::String, Rml::String> FontFamilyAliases;
        std::vector<std::filesystem::path> RegisteredFontPaths;
        std::vector<Rml::CallbackTexture> GeneratedTextures;
        int Version = 1;
    };

    FJSRmlUiSystemInterface GRmlUiSystemInterface;
    FJSRmlUiFileInterface GRmlUiFileInterface;
    FJSRmlUiGdiFontEngine GRmlUiFontEngine;
}

bool FRmlUiRuntimeModule::Initialize()
{
    if (bInitialized)
    {
        return true;
    }

    Rml::SetSystemInterface(&GRmlUiSystemInterface);
    Rml::SetFileInterface(&GRmlUiFileInterface);
    Rml::SetFontEngineInterface(&GRmlUiFontEngine);
    bInitialized = Rml::Initialise();
    if (!bInitialized)
    {
        UE_LOG_ERROR("[RmlUi] Failed to initialize RmlUi core.");
        return false;
    }

    Rml::LoadFontFace("C:/Windows/Fonts/malgun.ttf", "Malgun Gothic", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, true);
    Rml::LoadFontFace("Asset/UIFont/Nexon/NEXONLv1GothicRegular.ttf", "Nexon Lv1 Gothic", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, true);
    Rml::LoadFontFace("Asset/UIFont/Nexon/NEXONLv1GothicBold.ttf", "Nexon Lv1 Gothic", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold, false);
    UE_LOG("[RmlUi] Initialized RmlUi core: %s", Rml::GetVersion().c_str());
    return true;
}

void FRmlUiRuntimeModule::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    GRmlUiFontEngine.ReleaseCachedRenderResources();
    Rml::Shutdown();
    bInitialized = false;
    UE_LOG("[RmlUi] Shutdown RmlUi core.");
}

void FRmlUiRuntimeModule::ReleaseCachedFontRenderResources()
{
    GRmlUiFontEngine.ReleaseCachedRenderResources();
}
