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
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
    class FNipsRmlUiSystemInterface final : public Rml::SystemInterface
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

    class FNipsRmlUiFileInterface final : public Rml::FileInterface
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

    class FNipsRmlUiGdiFontEngine final : public Rml::FontEngineInterface
    {
    public:
        bool LoadFontFace(const Rml::String& FileName, int FaceIndex, bool bFallbackFace, Rml::Style::FontWeight Weight) override
        {
            (void)FaceIndex;
            (void)bFallbackFace;
            (void)Weight;
            UE_LOG("[RmlUi] Registered GDI font face request: %s", FileName.c_str());
            return true;
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
            if (!Family.empty())
            {
                DefaultFamily = Family;
            }
            UE_LOG("[RmlUi] Registered GDI font face request: %s Family=%s", FileName.c_str(), Family.c_str());
            return true;
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
            if (!Family.empty())
            {
                DefaultFamily = Family;
            }
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
            if (!Family.empty())
            {
                DefaultFamily = Family;
            }

            const int ClampedSize = std::max(Size, 8);
            EnsureMetrics(ClampedSize);
            return static_cast<Rml::FontFaceHandle>(ClampedSize);
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

            const int FontSize = HandleToSize(Handle);
            HFONT Font = CreateGdiFont(FontSize);
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

            const int FontSize = HandleToSize(FaceHandle);
            const Rml::FontMetrics& Metrics = EnsureMetrics(FontSize);
            const int Width = std::max(GetStringWidth(FaceHandle, String, TextShapingContext), 1);
            const int Height = std::max(static_cast<int>(Metrics.line_spacing + 2.0f), FontSize + 4);
            std::vector<Rml::byte> Pixels(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4, 0);

            RenderTextToPixels(Wide, FontSize, Width, Height, Colour, Opacity, Pixels);

            const Rml::Vector2i Dimensions(Width, Height);
            Rml::CallbackTexture TextureResource = RenderManager.MakeCallbackTexture(
                [Pixels = std::move(Pixels), Dimensions](const Rml::CallbackTextureInterface& TextureInterface)
                {
                    return TextureInterface.GenerateTexture(Rml::Span<const Rml::byte>(Pixels.data(), Pixels.size()), Dimensions);
                });
            Rml::Texture Texture = TextureResource;
            GeneratedTextures.push_back(std::move(TextureResource));

            Rml::Mesh Mesh;
            const float Left = Position.x;
            const float Top = Position.y - Metrics.ascent;
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
            return Width;
        }

        int GetVersion(Rml::FontFaceHandle Handle) override
        {
            (void)Handle;
            return Version;
        }

        void ReleaseFontResources() override
        {
            GeneratedTextures.clear();
            ++Version;
        }

    private:
        int HandleToSize(Rml::FontFaceHandle Handle) const
        {
            return std::max(static_cast<int>(Handle), 8);
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

        HFONT CreateGdiFont(int Size) const
        {
            const std::wstring Family = Utf8ToWide(DefaultFamily.empty() ? Rml::String("Malgun Gothic") : DefaultFamily);
            return ::CreateFontW(
                -Size,
                0,
                0,
                0,
                FW_NORMAL,
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

        void RenderTextToPixels(
            const std::wstring& Text,
            int FontSize,
            int Width,
            int Height,
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
            HFONT Font = CreateGdiFont(FontSize);
            HGDIOBJ OldFont = ::SelectObject(DC, Font);
            RECT Rect{ 0, 0, Width, Height };
            HBRUSH BlackBrush = ::CreateSolidBrush(RGB(0, 0, 0));
            ::FillRect(DC, &Rect, BlackBrush);
            ::DeleteObject(BlackBrush);
            ::SetBkMode(DC, TRANSPARENT);
            ::SetTextColor(DC, RGB(255, 255, 255));
            ::DrawTextW(DC, Text.c_str(), static_cast<int>(Text.size()), &Rect, DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE);

            const Rml::byte* Source = static_cast<const Rml::byte*>(Bits);
            const float OpacityScale = std::clamp(Opacity, 0.0f, 1.0f);
            for (int Y = 0; Y < Height; ++Y)
            {
                for (int X = 0; X < Width; ++X)
                {
                    const size_t Index = (static_cast<size_t>(Y) * Width + X) * 4;
                    const Rml::byte Coverage = std::max(Source[Index + 0], std::max(Source[Index + 1], Source[Index + 2]));
                    const float AlphaScale = (Coverage / 255.0f) * OpacityScale;
                    Pixels[Index + 0] = static_cast<Rml::byte>(Colour.red * AlphaScale);
                    Pixels[Index + 1] = static_cast<Rml::byte>(Colour.green * AlphaScale);
                    Pixels[Index + 2] = static_cast<Rml::byte>(Colour.blue * AlphaScale);
                    Pixels[Index + 3] = static_cast<Rml::byte>(Colour.alpha * AlphaScale);
                }
            }

            ::SelectObject(DC, OldFont);
            ::SelectObject(DC, OldBitmap);
            ::DeleteObject(Font);
            ::DeleteObject(Bitmap);
            ::DeleteDC(DC);
        }

        Rml::String DefaultFamily = "Malgun Gothic";
        std::unordered_map<int, Rml::FontMetrics> MetricsBySize;
        std::vector<Rml::CallbackTexture> GeneratedTextures;
        int Version = 1;
    };

    FNipsRmlUiSystemInterface GRmlUiSystemInterface;
    FNipsRmlUiFileInterface GRmlUiFileInterface;
    FNipsRmlUiGdiFontEngine GRmlUiFontEngine;
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
    UE_LOG("[RmlUi] Initialized RmlUi core: %s", Rml::GetVersion().c_str());
    return true;
}

void FRmlUiRuntimeModule::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    Rml::Shutdown();
    bInitialized = false;
    UE_LOG("[RmlUi] Shutdown RmlUi core.");
}
