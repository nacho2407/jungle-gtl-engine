#include "Editor/UI/Asset/UI/UIEditorSerializer.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	struct FTextPatch
	{
		int32 Begin = 0;
		int32 End = 0;
		FString Replacement;
	};

	struct FAttributeSpan
	{
		FString Name;
		FString Value;
		FSourceRange FullRange;
		FSourceRange ValueRange;
		char QuoteChar = '"';
	};

	struct FCssRuleInfo
	{
		int32 StyleSheetIndex = -1;
		int32 RuleBegin = -1;
		int32 RuleEnd = -1;
		int32 InsertPos = -1;
	};

	void SetUIEditorError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		return Value;
	}

	bool EqualsIgnoreCase(const FString& A, const FString& B)
	{
		return ToLowerCopy(A) == ToLowerCopy(B);
	}

	FString TrimCopy(FString Value)
	{
		auto IsSpace = [](unsigned char C) { return std::isspace(C) != 0; };
		Value.erase(Value.begin(), std::find_if(Value.begin(), Value.end(), [&](unsigned char C) { return !IsSpace(C); }));
		Value.erase(std::find_if(Value.rbegin(), Value.rend(), [&](unsigned char C) { return !IsSpace(C); }).base(), Value.end());
		return Value;
	}

	FString DecodeXml(FString Value)
	{
		struct FReplacement
		{
			const char* From;
			const char* To;
		};
		const FReplacement Replacements[] = {
			{ "&lt;", "<" },
			{ "&gt;", ">" },
			{ "&quot;", "\"" },
			{ "&apos;", "'" },
			{ "&amp;", "&" },
		};

		for (const FReplacement& Replacement : Replacements)
		{
			size_t Pos = 0;
			while ((Pos = Value.find(Replacement.From, Pos)) != FString::npos)
			{
				Value.replace(Pos, std::strlen(Replacement.From), Replacement.To);
				Pos += std::strlen(Replacement.To);
			}
		}
		return Value;
	}

	FString EncodeXml(const FString& Value)
	{
		FString Result;
		Result.reserve(Value.size());
		for (char C : Value)
		{
			switch (C)
			{
			case '&': Result += "&amp;"; break;
			case '<': Result += "&lt;"; break;
			case '>': Result += "&gt;"; break;
			case '"': Result += "&quot;"; break;
			case '\'': Result += "&apos;"; break;
			default: Result.push_back(C); break;
			}
		}
		return Result;
	}

	bool ReadTextFile(const std::filesystem::path& Path, FString& OutText, FString* OutError)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File)
		{
			SetUIEditorError(OutError, "Failed to open RML file.");
			return false;
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		OutText = Buffer.str();
		return true;
	}

	bool WriteTextFile(const std::filesystem::path& Path, const FString& Text, FString* OutError)
	{
		std::ofstream File(Path, std::ios::binary | std::ios::trunc);
		if (!File)
		{
			SetUIEditorError(OutError, "Failed to write RML file.");
			return false;
		}

		File.write(Text.data(), static_cast<std::streamsize>(Text.size()));
		if (!File)
		{
			SetUIEditorError(OutError, "Failed while writing RML file.");
			return false;
		}
		return true;
	}

	bool EndsWithIgnoreCase(const FString& Value, const FString& Suffix)
	{
		if (Suffix.size() > Value.size())
		{
			return false;
		}
		return ToLowerCopy(Value.substr(Value.size() - Suffix.size())) == ToLowerCopy(Suffix);
	}

	std::filesystem::path ResolveLinkedPath(const std::filesystem::path& DocumentPath, const FString& Href)
	{
		std::filesystem::path LinkedPath(FPaths::ToWide(Href));
		if (LinkedPath.is_relative())
		{
			LinkedPath = DocumentPath.parent_path() / LinkedPath;
		}
		return LinkedPath.lexically_normal();
	}

	FString MakeRelativeHref(const std::filesystem::path& DocumentPath, const std::filesystem::path& LinkedPath)
	{
		std::error_code Ec;
		std::filesystem::path Relative = std::filesystem::relative(LinkedPath, DocumentPath.parent_path(), Ec);
		if (Ec)
		{
			Relative = LinkedPath;
		}
		return FPaths::ToUtf8(Relative.generic_wstring());
	}

	bool BackupFile(const std::filesystem::path& Path, FString* OutError)
	{
		if (!std::filesystem::exists(Path))
		{
			return true;
		}

		std::error_code Ec;
		const std::filesystem::path BackupPath = Path.wstring() + L".bak";
		std::filesystem::copy_file(Path, BackupPath, std::filesystem::copy_options::overwrite_existing, Ec);
		if (Ec)
		{
			SetUIEditorError(OutError, "Failed to create RML backup: " + Ec.message());
			return false;
		}
		return true;
	}

	void ApplyPatchesDescending(FString& Source, TArray<FTextPatch>& Patches)
	{
		std::sort(Patches.begin(), Patches.end(),
			[](const FTextPatch& A, const FTextPatch& B)
			{
				return A.Begin > B.Begin;
			});

		for (const FTextPatch& Patch : Patches)
		{
			if (Patch.Begin < 0 || Patch.End < Patch.Begin || Patch.End > static_cast<int32>(Source.size()))
			{
				continue;
			}
			Source.replace(Source.begin() + Patch.Begin, Source.begin() + Patch.End, Patch.Replacement);
		}
	}

	int32 FindCaseInsensitive(const FString& Source, const FString& Needle, int32 Start = 0)
	{
		if (Needle.empty() || Start < 0 || Start >= static_cast<int32>(Source.size()))
		{
			return -1;
		}

		const FString LowerSource = ToLowerCopy(Source);
		const FString LowerNeedle = ToLowerCopy(Needle);
		const size_t Pos = LowerSource.find(LowerNeedle, static_cast<size_t>(Start));
		return Pos == FString::npos ? -1 : static_cast<int32>(Pos);
	}

	bool IsNameChar(char C)
	{
		return std::isalnum(static_cast<unsigned char>(C)) || C == '-' || C == '_' || C == ':';
	}

	bool IsEditableTag(const FString& TagName)
	{
		const FString Lower = ToLowerCopy(TagName);
		return Lower == "div" || Lower == "span" || Lower == "button" || Lower == "p" || Lower == "img";
	}

	bool IsSelfClosingEditableTag(const FString& TagName)
	{
		const FString Lower = ToLowerCopy(TagName);
		return Lower == "img";
	}

	bool IsBlockedTag(const FString& TagName)
	{
		const FString Lower = ToLowerCopy(TagName);
		return Lower == "style" || Lower == "script" || Lower == "head" || Lower == "body" || Lower == "link";
	}

	bool ParseStartTag(const FString& Source, int32 TagBegin, FString& OutTagName, int32& OutTagEnd, bool& bOutSelfClosing)
	{
		if (TagBegin < 0 || TagBegin >= static_cast<int32>(Source.size()) || Source[TagBegin] != '<')
		{
			return false;
		}

		if (TagBegin + 1 >= static_cast<int32>(Source.size()))
		{
			return false;
		}

		const char Next = Source[TagBegin + 1];
		if (Next == '/' || Next == '!' || Next == '?')
		{
			return false;
		}

		int32 Cursor = TagBegin + 1;
		while (Cursor < static_cast<int32>(Source.size()) && std::isspace(static_cast<unsigned char>(Source[Cursor])))
		{
			++Cursor;
		}

		const int32 NameBegin = Cursor;
		while (Cursor < static_cast<int32>(Source.size()) && IsNameChar(Source[Cursor]))
		{
			++Cursor;
		}

		if (Cursor <= NameBegin)
		{
			return false;
		}

		OutTagName = Source.substr(NameBegin, Cursor - NameBegin);

		char Quote = '\0';
		for (; Cursor < static_cast<int32>(Source.size()); ++Cursor)
		{
			const char C = Source[Cursor];
			if (Quote != '\0')
			{
				if (C == Quote)
				{
					Quote = '\0';
				}
				continue;
			}
			if (C == '"' || C == '\'')
			{
				Quote = C;
				continue;
			}
			if (C == '>')
			{
				OutTagEnd = Cursor + 1;
				int32 Probe = Cursor - 1;
				while (Probe > TagBegin && std::isspace(static_cast<unsigned char>(Source[Probe])))
				{
					--Probe;
				}
				bOutSelfClosing = Probe > TagBegin && Source[Probe] == '/';
				return true;
			}
		}

		return false;
	}

	TArray<FAttributeSpan> ParseAttributes(const FString& Source, int32 TagBegin, int32 TagEnd, const FString& TagName)
	{
		TArray<FAttributeSpan> Attributes;
		int32 Cursor = TagBegin + 1 + static_cast<int32>(TagName.size());

		while (Cursor < TagEnd - 1)
		{
			while (Cursor < TagEnd - 1 && std::isspace(static_cast<unsigned char>(Source[Cursor])))
			{
				++Cursor;
			}
			if (Cursor >= TagEnd - 1 || Source[Cursor] == '/' || Source[Cursor] == '>')
			{
				break;
			}

			const int32 NameBegin = Cursor;
			while (Cursor < TagEnd - 1 && IsNameChar(Source[Cursor]))
			{
				++Cursor;
			}
			if (Cursor <= NameBegin)
			{
				++Cursor;
				continue;
			}

			FAttributeSpan Attribute;
			Attribute.Name = Source.substr(NameBegin, Cursor - NameBegin);
			Attribute.FullRange.Begin = NameBegin;

			while (Cursor < TagEnd - 1 && std::isspace(static_cast<unsigned char>(Source[Cursor])))
			{
				++Cursor;
			}

			if (Cursor >= TagEnd - 1 || Source[Cursor] != '=')
			{
				Attribute.FullRange.End = Cursor;
				Attributes.push_back(Attribute);
				continue;
			}
			++Cursor;

			while (Cursor < TagEnd - 1 && std::isspace(static_cast<unsigned char>(Source[Cursor])))
			{
				++Cursor;
			}

			if (Cursor < TagEnd - 1 && (Source[Cursor] == '"' || Source[Cursor] == '\''))
			{
				Attribute.QuoteChar = Source[Cursor];
				++Cursor;
				Attribute.ValueRange.Begin = Cursor;
				while (Cursor < TagEnd - 1 && Source[Cursor] != Attribute.QuoteChar)
				{
					++Cursor;
				}
				Attribute.ValueRange.End = Cursor;
				Attribute.Value = Source.substr(Attribute.ValueRange.Begin, Attribute.ValueRange.End - Attribute.ValueRange.Begin);
				if (Cursor < TagEnd - 1)
				{
					++Cursor;
				}
				Attribute.FullRange.End = Cursor;
			}
			else
			{
				Attribute.ValueRange.Begin = Cursor;
				while (Cursor < TagEnd - 1 && !std::isspace(static_cast<unsigned char>(Source[Cursor])) && Source[Cursor] != '>')
				{
					++Cursor;
				}
				Attribute.ValueRange.End = Cursor;
				Attribute.Value = Source.substr(Attribute.ValueRange.Begin, Attribute.ValueRange.End - Attribute.ValueRange.Begin);
				Attribute.FullRange.End = Cursor;
			}

			Attributes.push_back(Attribute);
		}

		return Attributes;
	}

	const FAttributeSpan* FindAttribute(const TArray<FAttributeSpan>& Attributes, const FString& Name)
	{
		for (const FAttributeSpan& Attribute : Attributes)
		{
			if (EqualsIgnoreCase(Attribute.Name, Name))
			{
				return &Attribute;
			}
		}
		return nullptr;
	}

	bool PrepareLinkedStyleSheets(const std::filesystem::path& DocumentPath, FString& Rml, FUIEditorDocument& Document, FString* OutError, bool bForceSourceReload)
	{
		TArray<FTextPatch> Patches;
		int32 Cursor = 0;
		while (Cursor < static_cast<int32>(Rml.size()))
		{
			const size_t Found = Rml.find('<', static_cast<size_t>(Cursor));
			if (Found == FString::npos)
			{
				break;
			}

			const int32 TagBegin = static_cast<int32>(Found);
			FString TagName;
			int32 TagEnd = -1;
			bool bSelfClosing = false;
			if (!ParseStartTag(Rml, TagBegin, TagName, TagEnd, bSelfClosing))
			{
				Cursor = TagBegin + 1;
				continue;
			}

			if (!EqualsIgnoreCase(TagName, "link"))
			{
				Cursor = TagEnd;
				continue;
			}

			const TArray<FAttributeSpan> Attributes = ParseAttributes(Rml, TagBegin, TagEnd, TagName);
			const FAttributeSpan* HrefAttribute = FindAttribute(Attributes, "href");
			if (!HrefAttribute || HrefAttribute->Value.empty())
			{
				Cursor = TagEnd;
				continue;
			}

			FString Href = HrefAttribute->Value;
			if (!EndsWithIgnoreCase(Href, ".rcss") && !EndsWithIgnoreCase(Href, ".rcss.bak"))
			{
				Cursor = TagEnd;
				continue;
			}

			std::filesystem::path DraftPath = ResolveLinkedPath(DocumentPath, Href);
			std::filesystem::path SourcePath = DraftPath;
			if (EndsWithIgnoreCase(FPaths::ToUtf8(SourcePath.generic_wstring()), ".bak"))
			{
				FString SourceUtf8 = FPaths::ToUtf8(SourcePath.wstring());
				SourceUtf8.resize(SourceUtf8.size() - 4);
				SourcePath = std::filesystem::path(FPaths::ToWide(SourceUtf8));
			}
			else
			{
				DraftPath = SourcePath.wstring() + L".bak";
				const FString DraftHref = MakeRelativeHref(DocumentPath, DraftPath);
				Patches.push_back({ HrefAttribute->ValueRange.Begin, HrefAttribute->ValueRange.End, DraftHref });
			}

			FString CssSource;
			if (bForceSourceReload && std::filesystem::exists(SourcePath))
			{
				if (!ReadTextFile(SourcePath, CssSource, OutError) || !WriteTextFile(DraftPath, CssSource, OutError))
				{
					return false;
				}
			}
			else if (std::filesystem::exists(DraftPath))
			{
				if (!ReadTextFile(DraftPath, CssSource, OutError))
				{
					return false;
				}
			}
			else if (std::filesystem::exists(SourcePath))
			{
				if (!ReadTextFile(SourcePath, CssSource, OutError) || !WriteTextFile(DraftPath, CssSource, OutError))
				{
					return false;
				}
			}
			else
			{
				CssSource.clear();
				if (!WriteTextFile(DraftPath, CssSource, OutError))
				{
					return false;
				}
			}

			bool bAlreadyAdded = false;
			for (const FUIEditorStyleSheet& StyleSheet : Document.StyleSheets)
			{
				if (StyleSheet.DraftPath == DraftPath)
				{
					bAlreadyAdded = true;
					break;
				}
			}
			if (!bAlreadyAdded)
			{
				FUIEditorStyleSheet StyleSheet;
				StyleSheet.SourcePath = SourcePath;
				StyleSheet.DraftPath = DraftPath;
				StyleSheet.CurrentSource = CssSource;
				Document.StyleSheets.push_back(std::move(StyleSheet));
			}

			Cursor = TagEnd;
		}

		if (!Patches.empty())
		{
			ApplyPatchesDescending(Rml, Patches);
		}
		return true;
	}

	void RestoreSourceStyleSheetLinks(const std::filesystem::path& DocumentPath, FString& Rml)
	{
		TArray<FTextPatch> Patches;
		int32 Cursor = 0;
		while (Cursor < static_cast<int32>(Rml.size()))
		{
			const size_t Found = Rml.find('<', static_cast<size_t>(Cursor));
			if (Found == FString::npos)
			{
				break;
			}

			const int32 TagBegin = static_cast<int32>(Found);
			FString TagName;
			int32 TagEnd = -1;
			bool bSelfClosing = false;
			if (!ParseStartTag(Rml, TagBegin, TagName, TagEnd, bSelfClosing))
			{
				Cursor = TagBegin + 1;
				continue;
			}

			if (!EqualsIgnoreCase(TagName, "link"))
			{
				Cursor = TagEnd;
				continue;
			}

			const TArray<FAttributeSpan> Attributes = ParseAttributes(Rml, TagBegin, TagEnd, TagName);
			const FAttributeSpan* HrefAttribute = FindAttribute(Attributes, "href");
			if (!HrefAttribute || !EndsWithIgnoreCase(HrefAttribute->Value, ".rcss.bak"))
			{
				Cursor = TagEnd;
				continue;
			}

			const std::filesystem::path DraftPath = ResolveLinkedPath(DocumentPath, HrefAttribute->Value);
			FString SourceUtf8 = FPaths::ToUtf8(DraftPath.wstring());
			SourceUtf8.resize(SourceUtf8.size() - 4);
			const std::filesystem::path SourcePath(FPaths::ToWide(SourceUtf8));
			Patches.push_back({ HrefAttribute->ValueRange.Begin, HrefAttribute->ValueRange.End, MakeRelativeHref(DocumentPath, SourcePath) });
			Cursor = TagEnd;
		}

		if (!Patches.empty())
		{
			ApplyPatchesDescending(Rml, Patches);
		}
	}

	int32 FindMatchingEndTag(const FString& Source, const FString& TagName, int32 SearchStart)
	{
		const FString LowerTag = ToLowerCopy(TagName);
		int32 Depth = 1;
		int32 Cursor = SearchStart;

		while (Cursor >= 0 && Cursor < static_cast<int32>(Source.size()))
		{
			const size_t Pos = Source.find('<', static_cast<size_t>(Cursor));
			if (Pos == FString::npos)
			{
				return -1;
			}

			Cursor = static_cast<int32>(Pos);
			if (Cursor + 1 < static_cast<int32>(Source.size()) && Source[Cursor + 1] == '/')
			{
				int32 NameBegin = Cursor + 2;
				while (NameBegin < static_cast<int32>(Source.size()) && std::isspace(static_cast<unsigned char>(Source[NameBegin])))
				{
					++NameBegin;
				}
				int32 NameEnd = NameBegin;
				while (NameEnd < static_cast<int32>(Source.size()) && IsNameChar(Source[NameEnd]))
				{
					++NameEnd;
				}
				if (ToLowerCopy(Source.substr(NameBegin, NameEnd - NameBegin)) == LowerTag)
				{
					--Depth;
					if (Depth == 0)
					{
						return Cursor;
					}
				}
			}
			else
			{
				FString ChildTagName;
				int32 ChildTagEnd = -1;
				bool bSelfClosing = false;
				if (ParseStartTag(Source, Cursor, ChildTagName, ChildTagEnd, bSelfClosing) &&
					ToLowerCopy(ChildTagName) == LowerTag &&
					!bSelfClosing)
				{
					++Depth;
					Cursor = ChildTagEnd;
					continue;
				}
			}

			++Cursor;
		}

		return -1;
	}

	FStyleValueSpan ParseStyleProperty(const FString& Source, const FAttributeSpan* StyleAttribute, const FString& PropertyName)
	{
		FStyleValueSpan Result;
		Result.PropertyName = PropertyName;
		if (!StyleAttribute || !StyleAttribute->ValueRange.IsValid())
		{
			return Result;
		}

		FString LowerStyle = ToLowerCopy(StyleAttribute->Value);
		FString LowerProperty = ToLowerCopy(PropertyName);
		size_t Pos = 0;
		while ((Pos = LowerStyle.find(LowerProperty, Pos)) != FString::npos)
		{
			const bool bLeftBoundary = Pos == 0 || LowerStyle[Pos - 1] == ';' || std::isspace(static_cast<unsigned char>(LowerStyle[Pos - 1]));
			size_t AfterName = Pos + LowerProperty.size();
			while (AfterName < LowerStyle.size() && std::isspace(static_cast<unsigned char>(LowerStyle[AfterName])))
			{
				++AfterName;
			}
			if (!bLeftBoundary || AfterName >= LowerStyle.size() || LowerStyle[AfterName] != ':')
			{
				Pos = AfterName;
				continue;
			}

			size_t ValueBegin = AfterName + 1;
			while (ValueBegin < LowerStyle.size() && std::isspace(static_cast<unsigned char>(LowerStyle[ValueBegin])))
			{
				++ValueBegin;
			}

			size_t ValueEnd = ValueBegin;
			while (ValueEnd < LowerStyle.size() && LowerStyle[ValueEnd] != ';')
			{
				++ValueEnd;
			}
			while (ValueEnd > ValueBegin && std::isspace(static_cast<unsigned char>(LowerStyle[ValueEnd - 1])))
			{
				--ValueEnd;
			}

			Result.bExistsInSource = true;
			Result.ValueRange.Begin = StyleAttribute->ValueRange.Begin + static_cast<int32>(ValueBegin);
			Result.ValueRange.End = StyleAttribute->ValueRange.Begin + static_cast<int32>(ValueEnd);
			Result.OriginalValue = Source.substr(Result.ValueRange.Begin, Result.ValueRange.End - Result.ValueRange.Begin);
			return Result;
		}

		return Result;
	}

	FStyleValueSpan ParseCssRuleProperty(const FString& Source, int32 RuleBegin, int32 RuleEnd, const FString& PropertyName, int32 StyleSheetIndex)
	{
		FStyleValueSpan Result;
		Result.PropertyName = PropertyName;
		Result.StyleSheetIndex = StyleSheetIndex;
		if (RuleBegin < 0 || RuleEnd <= RuleBegin || RuleEnd > static_cast<int32>(Source.size()))
		{
			return Result;
		}

		FString RuleSource = Source.substr(RuleBegin, RuleEnd - RuleBegin);
		FString LowerRule = ToLowerCopy(RuleSource);
		FString LowerProperty = ToLowerCopy(PropertyName);
		size_t Pos = 0;
		while ((Pos = LowerRule.find(LowerProperty, Pos)) != FString::npos)
		{
			const bool bLeftBoundary = Pos == 0 || LowerRule[Pos - 1] == ';' || LowerRule[Pos - 1] == '{' || std::isspace(static_cast<unsigned char>(LowerRule[Pos - 1]));
			size_t AfterName = Pos + LowerProperty.size();
			while (AfterName < LowerRule.size() && std::isspace(static_cast<unsigned char>(LowerRule[AfterName])))
			{
				++AfterName;
			}
			if (!bLeftBoundary || AfterName >= LowerRule.size() || LowerRule[AfterName] != ':')
			{
				Pos = AfterName;
				continue;
			}

			size_t ValueBegin = AfterName + 1;
			while (ValueBegin < LowerRule.size() && std::isspace(static_cast<unsigned char>(LowerRule[ValueBegin])))
			{
				++ValueBegin;
			}

			size_t ValueEnd = ValueBegin;
			while (ValueEnd < LowerRule.size() && LowerRule[ValueEnd] != ';' && LowerRule[ValueEnd] != '}')
			{
				++ValueEnd;
			}
			while (ValueEnd > ValueBegin && std::isspace(static_cast<unsigned char>(LowerRule[ValueEnd - 1])))
			{
				--ValueEnd;
			}

			Result.bExistsInSource = true;
			Result.ValueRange.Begin = RuleBegin + static_cast<int32>(ValueBegin);
			Result.ValueRange.End = RuleBegin + static_cast<int32>(ValueEnd);
			Result.OriginalValue = Source.substr(Result.ValueRange.Begin, Result.ValueRange.End - Result.ValueRange.Begin);
			return Result;
		}

		return Result;
	}

	FCssRuleInfo FindCssRule(const FUIEditorDocument& Document, const FString& Selector)
	{
		for (int32 StyleSheetIndex = 0; StyleSheetIndex < static_cast<int32>(Document.StyleSheets.size()); ++StyleSheetIndex)
		{
			const FString& Css = Document.StyleSheets[StyleSheetIndex].CurrentSource;
			size_t Search = 0;
			while ((Search = Css.find(Selector, Search)) != FString::npos)
			{
				const size_t BraceOpen = Css.find('{', Search + Selector.size());
				if (BraceOpen == FString::npos)
				{
					break;
				}
				const FString Between = Css.substr(Search + Selector.size(), BraceOpen - (Search + Selector.size()));
				if (Between.find_first_not_of(" \t\r\n") != FString::npos)
				{
					Search = Search + Selector.size();
					continue;
				}
				const size_t BraceClose = Css.find('}', BraceOpen + 1);
				if (BraceClose == FString::npos)
				{
					break;
				}

				FCssRuleInfo Info;
				Info.StyleSheetIndex = StyleSheetIndex;
				Info.RuleBegin = static_cast<int32>(BraceOpen + 1);
				Info.RuleEnd = static_cast<int32>(BraceClose);
				Info.InsertPos = static_cast<int32>(BraceClose);
				return Info;
			}
		}
		return {};
	}

	FCssRuleInfo FindCssIdRule(const FUIEditorDocument& Document, const FString& Id)
	{
		return FindCssRule(Document, "#" + Id);
	}

	FCssRuleInfo FindCssClassRule(const FUIEditorDocument& Document, const FString& ClassName)
	{
		return FindCssRule(Document, "." + ClassName);
	}

	void ApplyCssStyleIfMissing(FUIEditorTextElement& Element, FStyleValueSpan& InlineSpan, const FStyleValueSpan& CssSpan)
	{
		(void)Element;
		if (!InlineSpan.bExistsInSource && CssSpan.bExistsInSource)
		{
			InlineSpan = CssSpan;
		}
	}

	void ApplyCssRuleStylesIfMissing(FUIEditorTextElement& Element, const FUIEditorDocument& Document, const FCssRuleInfo& CssRule)
	{
		if (CssRule.StyleSheetIndex < 0 || CssRule.StyleSheetIndex >= static_cast<int32>(Document.StyleSheets.size()))
		{
			return;
		}

		const FString& Css = Document.StyleSheets[CssRule.StyleSheetIndex].CurrentSource;
		ApplyCssStyleIfMissing(Element, Element.PositionStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "position", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.LeftStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "left", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.TopStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "top", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.WidthStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "width", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.HeightStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "height", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.FontSizeStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "font-size", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.FontWeightStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "font-weight", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.TextAlignStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "text-align", CssRule.StyleSheetIndex));
		ApplyCssStyleIfMissing(Element, Element.ColorStyle, ParseCssRuleProperty(Css, CssRule.RuleBegin, CssRule.RuleEnd, "color", CssRule.StyleSheetIndex));

		if (Element.CssRuleStyleSheetIndex < 0)
		{
			Element.CssRuleStyleSheetIndex = CssRule.StyleSheetIndex;
			Element.CssRuleInsertPos = CssRule.InsertPos;
		}
	}

	float ParseStyleFloat(const FStyleValueSpan& Span, float DefaultValue)
	{
		if (!Span.bExistsInSource)
		{
			return DefaultValue;
		}
		try
		{
			return std::stof(Span.OriginalValue);
		}
		catch (...)
		{
			return DefaultValue;
		}
	}

	bool IsPercentStyle(const FStyleValueSpan& Span)
	{
		return Span.bExistsInSource && Span.OriginalValue.find('%') != FString::npos;
	}

	int HexDigitValue(char C)
	{
		if (C >= '0' && C <= '9') return C - '0';
		if (C >= 'a' && C <= 'f') return 10 + C - 'a';
		if (C >= 'A' && C <= 'F') return 10 + C - 'A';
		return -1;
	}

	bool ParseHexByte(const FString& Value, size_t Offset, int& OutByte)
	{
		if (Offset + 1 >= Value.size())
		{
			return false;
		}

		const int High = HexDigitValue(Value[Offset]);
		const int Low = HexDigitValue(Value[Offset + 1]);
		if (High < 0 || Low < 0)
		{
			return false;
		}

		OutByte = High * 16 + Low;
		return true;
	}

	bool ParseCssColor(const FString& CssColor, float OutColor[4])
	{
		const FString Value = TrimCopy(CssColor);
		const FString Lower = ToLowerCopy(Value);
		OutColor[0] = 1.0f;
		OutColor[1] = 1.0f;
		OutColor[2] = 1.0f;
		OutColor[3] = 1.0f;

		if (Lower == "white")
		{
			return true;
		}
		if (Lower == "black")
		{
			OutColor[0] = 0.0f;
			OutColor[1] = 0.0f;
			OutColor[2] = 0.0f;
			return true;
		}
		if (Lower == "transparent")
		{
			OutColor[3] = 0.0f;
			return true;
		}

		if (Value.size() == 4 && Value[0] == '#')
		{
			const int R = HexDigitValue(Value[1]);
			const int G = HexDigitValue(Value[2]);
			const int B = HexDigitValue(Value[3]);
			if (R < 0 || G < 0 || B < 0)
			{
				return false;
			}
			OutColor[0] = static_cast<float>(R * 17) / 255.0f;
			OutColor[1] = static_cast<float>(G * 17) / 255.0f;
			OutColor[2] = static_cast<float>(B * 17) / 255.0f;
			return true;
		}

		if ((Value.size() == 7 || Value.size() == 9) && Value[0] == '#')
		{
			int R = 255;
			int G = 255;
			int B = 255;
			int A = 255;
			if (!ParseHexByte(Value, 1, R) || !ParseHexByte(Value, 3, G) || !ParseHexByte(Value, 5, B))
			{
				return false;
			}
			if (Value.size() == 9 && !ParseHexByte(Value, 7, A))
			{
				return false;
			}
			OutColor[0] = static_cast<float>(R) / 255.0f;
			OutColor[1] = static_cast<float>(G) / 255.0f;
			OutColor[2] = static_cast<float>(B) / 255.0f;
			OutColor[3] = static_cast<float>(A) / 255.0f;
			return true;
		}

		const bool bRgb = Lower.rfind("rgb(", 0) == 0 || Lower.rfind("rgba(", 0) == 0;
		if (bRgb)
		{
			const size_t Open = Value.find('(');
			const size_t Close = Value.find(')', Open == FString::npos ? 0 : Open);
			if (Open == FString::npos || Close == FString::npos || Close <= Open)
			{
				return false;
			}

			FString Inner = Value.substr(Open + 1, Close - Open - 1);
			TArray<float> Components;
			size_t Start = 0;
			while (Start < Inner.size())
			{
				const size_t Comma = Inner.find(',', Start);
				const FString Token = TrimCopy(Inner.substr(Start, Comma == FString::npos ? FString::npos : Comma - Start));
				try
				{
					Components.push_back(std::stof(Token));
				}
				catch (...)
				{
					return false;
				}

				if (Comma == FString::npos)
				{
					break;
				}
				Start = Comma + 1;
			}

			if (Components.size() < 3)
			{
				return false;
			}

			OutColor[0] = std::clamp(Components[0] / 255.0f, 0.0f, 1.0f);
			OutColor[1] = std::clamp(Components[1] / 255.0f, 0.0f, 1.0f);
			OutColor[2] = std::clamp(Components[2] / 255.0f, 0.0f, 1.0f);
			OutColor[3] = Components.size() > 3 ? std::clamp(Components[3], 0.0f, 1.0f) : 1.0f;
			return true;
		}

		return false;
	}

	FString FormatCssColor(const float Color[4])
	{
		const int R = static_cast<int>(std::clamp(Color[0], 0.0f, 1.0f) * 255.0f + 0.5f);
		const int G = static_cast<int>(std::clamp(Color[1], 0.0f, 1.0f) * 255.0f + 0.5f);
		const int B = static_cast<int>(std::clamp(Color[2], 0.0f, 1.0f) * 255.0f + 0.5f);
		const float A = std::clamp(Color[3], 0.0f, 1.0f);

		char Buffer[64];
		if (A < 0.999f)
		{
			std::snprintf(Buffer, sizeof(Buffer), "rgba(%d, %d, %d, %.3f)", R, G, B, A);
			return Buffer;
		}

		std::snprintf(Buffer, sizeof(Buffer), "#%02X%02X%02X", R, G, B);
		return Buffer;
	}

	FString FormatFloatValue(float Value, const char* Unit)
	{
		char Buffer[64];
		std::snprintf(Buffer, sizeof(Buffer), "%.2f%s", Value, Unit);
		return Buffer;
	}

	FString FormatFontWeight(const FString& FontWeight)
	{
		if (FontWeight == "900" || FontWeight == "black")
		{
			return "900";
		}
		return FontWeight == "bold" ? "bold" : "normal";
	}

	FString FormatTextAlign(const FString& TextAlign)
	{
		const FString Lower = ToLowerCopy(TextAlign);
		if (Lower == "center" || Lower == "right")
		{
			return Lower;
		}
		return "left";
	}

	FString BuildStyleText(const FUIEditorTextElement& Element)
	{
		const char* LayoutUnit = Element.bUsePercentLayout ? "%" : "px";
		const float Width = (std::max)(1.0f, Element.Width);
		const float Height = (std::max)(1.0f, Element.Height);
		const float FontSize = (std::max)(1.0f, Element.FontSize);

		std::ostringstream Out;
		Out << "position: absolute; ";
		Out << "left: " << FormatFloatValue(Element.X, LayoutUnit) << "; ";
		Out << "top: " << FormatFloatValue(Element.Y, LayoutUnit) << "; ";
		Out << "width: " << FormatFloatValue(Width, LayoutUnit) << "; ";
		Out << "height: " << FormatFloatValue(Height, LayoutUnit) << "; ";
		Out << "font-size: " << FormatFloatValue(FontSize, "px") << "; ";
		Out << "font-weight: " << FormatFontWeight(Element.FontWeight) << "; ";
		Out << "text-align: " << FormatTextAlign(Element.TextAlign) << "; ";
		Out << "color: " << FormatCssColor(Element.Color) << ";";
		return Out.str();
	}

	FString BuildLayoutStyleText(const FUIEditorTextElement& Element)
	{
		const char* LayoutUnit = Element.bUsePercentLayout ? "%" : "px";
		const float Width = (std::max)(1.0f, Element.Width);
		const float Height = (std::max)(1.0f, Element.Height);

		std::ostringstream Out;
		Out << "position: absolute; ";
		Out << "left: " << FormatFloatValue(Element.X, LayoutUnit) << "; ";
		Out << "top: " << FormatFloatValue(Element.Y, LayoutUnit) << "; ";
		Out << "width: " << FormatFloatValue(Width, LayoutUnit) << "; ";
		Out << "height: " << FormatFloatValue(Height, LayoutUnit) << ";";
		return Out.str();
	}

	bool HasDirtyLayoutProperty(const FUIEditorTextElement& Element)
	{
		return Element.bXDirty || Element.bYDirty || Element.bWidthDirty || Element.bHeightDirty;
	}

	FString BuildDirtyStyleText(const FUIEditorTextElement& Element)
	{
		const char* LayoutUnit = Element.bUsePercentLayout ? "%" : "px";
		std::ostringstream Out;

		if (HasDirtyLayoutProperty(Element) && !Element.PositionStyle.bExistsInSource)
		{
			Out << "position: absolute; ";
		}
		if (Element.bXDirty)
		{
			Out << "left: " << FormatFloatValue(Element.X, LayoutUnit) << "; ";
		}
		if (Element.bYDirty)
		{
			Out << "top: " << FormatFloatValue(Element.Y, LayoutUnit) << "; ";
		}
		if (Element.bWidthDirty)
		{
			Out << "width: " << FormatFloatValue((std::max)(1.0f, Element.Width), LayoutUnit) << "; ";
		}
		if (Element.bHeightDirty)
		{
			Out << "height: " << FormatFloatValue((std::max)(1.0f, Element.Height), LayoutUnit) << "; ";
		}
		if (Element.bCanEditText && Element.bFontSizeDirty)
		{
			Out << "font-size: " << FormatFloatValue((std::max)(1.0f, Element.FontSize), "px") << "; ";
		}
		if (Element.bCanEditText && Element.bFontWeightDirty)
		{
			Out << "font-weight: " << FormatFontWeight(Element.FontWeight) << "; ";
		}
		if (Element.bCanEditText && Element.bTextAlignDirty)
		{
			Out << "text-align: " << FormatTextAlign(Element.TextAlign) << "; ";
		}
		if (Element.bCanEditText && Element.bColorDirty)
		{
			Out << "color: " << FormatCssColor(Element.Color) << "; ";
		}

		FString Result = Out.str();
		while (!Result.empty() && std::isspace(static_cast<unsigned char>(Result.back())))
		{
			Result.pop_back();
		}
		return Result;
	}

	int32 FindStyleInsertPosition(const FString& Source, const FUIEditorTextElement& Element)
	{
		int32 InsertPos = Element.OpenTagRange.End - 1;
		if (Element.bSelfClosing)
		{
			int32 Probe = InsertPos - 1;
			while (Probe > Element.OpenTagRange.Begin && std::isspace(static_cast<unsigned char>(Source[Probe])))
			{
				--Probe;
			}
			if (Probe > Element.OpenTagRange.Begin && Source[Probe] == '/')
			{
				InsertPos = Probe;
			}
		}
		return InsertPos;
	}

	void AddStyleValuePatchOrAppend(const FUIEditorTextElement& Element, const FStyleValueSpan& Span, const FString& Value, TArray<FTextPatch>& Patches, TArray<FString>& MissingProperties)
	{
		(void)Element;
		if (Span.bExistsInSource && Span.ValueRange.IsValid())
		{
			Patches.push_back({ Span.ValueRange.Begin, Span.ValueRange.End, Value });
		}
		else
		{
			MissingProperties.push_back(Span.PropertyName + ": " + Value);
		}
	}

	void AddStyleValuePatchOrAppend(
		const FUIEditorTextElement& Element,
		const FStyleValueSpan& Span,
		const FString& Value,
		TArray<FTextPatch>& RmlPatches,
		TArray<TArray<FTextPatch>>& CssPatches,
		TArray<FString>& MissingProperties)
	{
		(void)Element;
		if (Span.bExistsInSource && Span.ValueRange.IsValid())
		{
			if (Span.StyleSheetIndex >= 0 && Span.StyleSheetIndex < static_cast<int32>(CssPatches.size()))
			{
				CssPatches[Span.StyleSheetIndex].push_back({ Span.ValueRange.Begin, Span.ValueRange.End, Value });
			}
			else
			{
				RmlPatches.push_back({ Span.ValueRange.Begin, Span.ValueRange.End, Value });
			}
		}
		else
		{
			MissingProperties.push_back(Span.PropertyName + ": " + Value);
		}
	}

	int32 FindBodyInsertPosition(const FString& Source)
	{
		int32 InsertPos = FindCaseInsensitive(Source, "</body>");
		if (InsertPos >= 0)
		{
			return InsertPos;
		}
		InsertPos = FindCaseInsensitive(Source, "</rml>");
		if (InsertPos >= 0)
		{
			return InsertPos;
		}
		return static_cast<int32>(Source.size());
	}

	FString BuildInsertedElementHtml(const FUIEditorTextElement& Element)
	{
		std::ostringstream Out;
		Out << "\n    <div id=\"" << EncodeXml(Element.Id) << "\" data-ui-editor=\"text\" style=\"";
		Out << BuildStyleText(Element);
		Out << "\">";
		Out << EncodeXml(Element.Text);
		Out << "</div>\n";
		return Out.str();
	}
}

bool FUIEditorSerializer::Load(const std::filesystem::path& Path, FUIEditorDocument& OutDocument, FString* OutError, bool bForceSourceReload)
{
	FString Rml;
	FString SourceRml;
	if (std::filesystem::exists(Path))
	{
		if (!ReadTextFile(Path, SourceRml, OutError))
		{
			return false;
		}
	}
	else
	{
		SourceRml = "<rml>\n<head>\n    <title>New UI</title>\n</head>\n<body>\n</body>\n</rml>\n";
		if (!WriteTextFile(Path, SourceRml, OutError))
		{
			return false;
		}
	}

	const std::filesystem::path DraftPath = Path.wstring() + L".bak";
	if (!bForceSourceReload && std::filesystem::exists(DraftPath))
	{
		if (!ReadTextFile(DraftPath, Rml, OutError))
		{
			return false;
		}
	}
	else
	{
		Rml = SourceRml;
		if (!WriteTextFile(DraftPath, Rml, OutError))
		{
			return false;
		}
	}

	OutDocument = FUIEditorDocument {};
	OutDocument.SourcePath = Path;
	OutDocument.DraftPath = DraftPath;
	if (!PrepareLinkedStyleSheets(Path, Rml, OutDocument, OutError, bForceSourceReload))
	{
		return false;
	}
	if (!WriteTextFile(DraftPath, Rml, OutError))
	{
		return false;
	}
	OutDocument.OriginalSource = Rml;
	OutDocument.CurrentSource = Rml;
	OutDocument.bDirty = false;

	return ParseEditableTextElements(Rml, OutDocument, OutError);
}

bool FUIEditorSerializer::SaveDraft(FUIEditorDocument& Document, FString* OutError)
{
	if (Document.SourcePath.empty())
	{
		SetUIEditorError(OutError, "RML source path is empty.");
		return false;
	}
	if (Document.DraftPath.empty())
	{
		Document.DraftPath = Document.SourcePath.wstring() + L".bak";
	}

	FString Source = Document.CurrentSource.empty() ? Document.OriginalSource : Document.CurrentSource;
	TArray<FTextPatch> Patches;
	TArray<TArray<FTextPatch>> CssPatches;
	CssPatches.resize(Document.StyleSheets.size());

	for (const FSourceRange& DeletedRange : Document.DeletedElementRanges)
	{
		if (DeletedRange.IsValid())
		{
			Patches.push_back({ DeletedRange.Begin, DeletedRange.End, "" });
		}
	}

	for (const FUIEditorTextElement& Element : Document.TextElements)
	{
		if (Element.bPendingInsert)
		{
			const int32 InsertPos = FindBodyInsertPosition(Source);
			Patches.push_back({ InsertPos, InsertPos, BuildInsertedElementHtml(Element) });
			continue;
		}

		if (Element.bTextDirty && Element.bCanEditText && Element.InnerTextRange.IsValid())
		{
			Patches.push_back({ Element.InnerTextRange.Begin, Element.InnerTextRange.End, EncodeXml(Element.Text) });
		}

		if (!Element.bStyleDirty)
		{
			continue;
		}

		if (!Element.StyleAttributeValueRange.IsValid() && Element.CssRuleStyleSheetIndex < 0)
		{
			const FString StyleText = BuildDirtyStyleText(Element);
			if (!StyleText.empty())
			{
				const int32 InsertPos = FindStyleInsertPosition(Source, Element);
				Patches.push_back({ InsertPos, InsertPos, " style=\"" + StyleText + "\"" });
			}
			continue;
		}

		const char* LayoutUnit = Element.bUsePercentLayout ? "%" : "px";
		TArray<FString> MissingProperties;
		if (HasDirtyLayoutProperty(Element) && !Element.PositionStyle.bExistsInSource)
		{
			MissingProperties.push_back("position: absolute");
		}
		if (Element.bXDirty)
		{
			AddStyleValuePatchOrAppend(Element, Element.LeftStyle, FormatFloatValue(Element.X, LayoutUnit), Patches, CssPatches, MissingProperties);
		}
		if (Element.bYDirty)
		{
			AddStyleValuePatchOrAppend(Element, Element.TopStyle, FormatFloatValue(Element.Y, LayoutUnit), Patches, CssPatches, MissingProperties);
		}
		if (Element.bWidthDirty)
		{
			AddStyleValuePatchOrAppend(Element, Element.WidthStyle, FormatFloatValue((std::max)(1.0f, Element.Width), LayoutUnit), Patches, CssPatches, MissingProperties);
		}
		if (Element.bHeightDirty)
		{
			AddStyleValuePatchOrAppend(Element, Element.HeightStyle, FormatFloatValue((std::max)(1.0f, Element.Height), LayoutUnit), Patches, CssPatches, MissingProperties);
		}
		if (Element.bCanEditText)
		{
			if (Element.bFontSizeDirty)
			{
				AddStyleValuePatchOrAppend(Element, Element.FontSizeStyle, FormatFloatValue((std::max)(1.0f, Element.FontSize), "px"), Patches, CssPatches, MissingProperties);
			}
			if (Element.bFontWeightDirty)
			{
				AddStyleValuePatchOrAppend(Element, Element.FontWeightStyle, FormatFontWeight(Element.FontWeight), Patches, CssPatches, MissingProperties);
			}
			if (Element.bTextAlignDirty)
			{
				AddStyleValuePatchOrAppend(Element, Element.TextAlignStyle, FormatTextAlign(Element.TextAlign), Patches, CssPatches, MissingProperties);
			}
			if (Element.bColorDirty)
			{
				AddStyleValuePatchOrAppend(Element, Element.ColorStyle, FormatCssColor(Element.Color), Patches, CssPatches, MissingProperties);
			}
		}

		if (!MissingProperties.empty())
		{
			std::ostringstream AppendStyle;
			const bool bNeedsSeparator = Element.StyleAttributeValueRange.End > Element.StyleAttributeValueRange.Begin &&
				Source[Element.StyleAttributeValueRange.End - 1] != ';';
			if (bNeedsSeparator)
			{
				AppendStyle << ";";
			}
			for (const FString& Property : MissingProperties)
			{
				AppendStyle << " " << Property << ";";
			}
			if (Element.CssRuleStyleSheetIndex >= 0 && Element.CssRuleStyleSheetIndex < static_cast<int32>(CssPatches.size()) && Element.CssRuleInsertPos >= 0)
			{
				CssPatches[Element.CssRuleStyleSheetIndex].push_back({ Element.CssRuleInsertPos, Element.CssRuleInsertPos, AppendStyle.str() });
			}
			else
			{
				Patches.push_back({ Element.StyleAttributeValueRange.End, Element.StyleAttributeValueRange.End, AppendStyle.str() });
			}
		}
	}

	ApplyPatchesDescending(Source, Patches);

	for (int32 StyleSheetIndex = 0; StyleSheetIndex < static_cast<int32>(Document.StyleSheets.size()); ++StyleSheetIndex)
	{
		if (CssPatches[StyleSheetIndex].empty())
		{
			continue;
		}
		FString CssSource = Document.StyleSheets[StyleSheetIndex].CurrentSource;
		ApplyPatchesDescending(CssSource, CssPatches[StyleSheetIndex]);
		if (!WriteTextFile(Document.StyleSheets[StyleSheetIndex].DraftPath, CssSource, OutError))
		{
			return false;
		}
	}

	if (!WriteTextFile(Document.DraftPath, Source, OutError))
	{
		return false;
	}

	const std::filesystem::path ReloadPath = Document.SourcePath;
	return Load(ReloadPath, Document, OutError);
}

bool FUIEditorSerializer::Commit(FUIEditorDocument& Document, FString* OutError)
{
	if (!SaveDraft(Document, OutError))
	{
		return false;
	}
	if (Document.SourcePath.empty())
	{
		SetUIEditorError(OutError, "RML source path is empty.");
		return false;
	}
	if (Document.DraftPath.empty())
	{
		SetUIEditorError(OutError, "RML draft path is empty.");
		return false;
	}

	FString DraftSource;
	if (!ReadTextFile(Document.DraftPath, DraftSource, OutError))
	{
		return false;
	}
	RestoreSourceStyleSheetLinks(Document.SourcePath, DraftSource);
	if (!WriteTextFile(Document.SourcePath, DraftSource, OutError))
	{
		return false;
	}

	for (const FUIEditorStyleSheet& StyleSheet : Document.StyleSheets)
	{
		if (StyleSheet.SourcePath.empty() || StyleSheet.DraftPath.empty())
		{
			continue;
		}

		FString DraftCss;
		if (!ReadTextFile(StyleSheet.DraftPath, DraftCss, OutError))
		{
			return false;
		}
		if (!WriteTextFile(StyleSheet.SourcePath, DraftCss, OutError))
		{
			return false;
		}
	}

	return Load(Document.SourcePath, Document, OutError);
}

bool FUIEditorSerializer::ParseEditableTextElements(const FString& Rml, FUIEditorDocument& OutDocument, FString* OutError)
{
	(void)OutError;

	OutDocument.TextElements.clear();

	int32 Cursor = 0;
	while (Cursor < static_cast<int32>(Rml.size()))
	{
		const size_t Found = Rml.find('<', static_cast<size_t>(Cursor));
		if (Found == FString::npos)
		{
			break;
		}

		const int32 TagBegin = static_cast<int32>(Found);
		FString TagName;
		int32 TagEnd = -1;
		bool bSelfClosing = false;
		if (!ParseStartTag(Rml, TagBegin, TagName, TagEnd, bSelfClosing))
		{
			Cursor = TagBegin + 1;
			continue;
		}

		if (IsBlockedTag(TagName) || !IsEditableTag(TagName) || (bSelfClosing && !IsSelfClosingEditableTag(TagName)))
		{
			Cursor = TagEnd;
			continue;
		}

		const TArray<FAttributeSpan> Attributes = ParseAttributes(Rml, TagBegin, TagEnd, TagName);
		const FAttributeSpan* IdAttribute = FindAttribute(Attributes, "id");
		if (!IdAttribute || IdAttribute->Value.empty())
		{
			Cursor = TagEnd;
			continue;
		}
		const FAttributeSpan* SkipEditorAttribute = FindAttribute(Attributes, "data-ui-editor-skip");
		if (SkipEditorAttribute && EqualsIgnoreCase(TrimCopy(SkipEditorAttribute->Value), "true"))
		{
			Cursor = TagEnd;
			continue;
		}

		int32 CloseTagBegin = TagEnd;
		int32 CloseTagEnd = TagEnd;
		if (!bSelfClosing)
		{
			CloseTagBegin = FindMatchingEndTag(Rml, TagName, TagEnd);
			if (CloseTagBegin < 0)
			{
				Cursor = TagEnd;
				continue;
			}
			size_t CloseTagEndPos = Rml.find('>', static_cast<size_t>(CloseTagBegin));
			if (CloseTagEndPos == FString::npos)
			{
				Cursor = TagEnd;
				continue;
			}
			CloseTagEnd = static_cast<int32>(CloseTagEndPos) + 1;
		}

		const FAttributeSpan* ClassAttribute = FindAttribute(Attributes, "class");
		const FAttributeSpan* StyleAttribute = FindAttribute(Attributes, "style");
		const FString InnerSource = bSelfClosing ? FString {} : Rml.substr(TagEnd, CloseTagBegin - TagEnd);
		const bool bHasChildElements = InnerSource.find('<') != FString::npos;

		FUIEditorTextElement Element;
		Element.TagName = ToLowerCopy(TagName);
		Element.Id = DecodeXml(IdAttribute->Value);
		Element.ClassName = ClassAttribute ? DecodeXml(ClassAttribute->Value) : FString {};
		Element.bSelfClosing = bSelfClosing;
		Element.Text = bHasChildElements ? FString {} : DecodeXml(TrimCopy(InnerSource));
		Element.OpenTagRange = { TagBegin, TagEnd };
		Element.ElementRange = { TagBegin, CloseTagEnd };
		Element.InnerTextRange = bHasChildElements ? FSourceRange {} : FSourceRange { TagEnd, CloseTagBegin };
		Element.StyleAttributeValueRange = StyleAttribute ? StyleAttribute->ValueRange : FSourceRange {};
		Element.bCanEditText = !bSelfClosing && !bHasChildElements;

		Element.PositionStyle = ParseStyleProperty(Rml, StyleAttribute, "position");
		Element.LeftStyle = ParseStyleProperty(Rml, StyleAttribute, "left");
		Element.TopStyle = ParseStyleProperty(Rml, StyleAttribute, "top");
		Element.WidthStyle = ParseStyleProperty(Rml, StyleAttribute, "width");
		Element.HeightStyle = ParseStyleProperty(Rml, StyleAttribute, "height");
		Element.FontSizeStyle = ParseStyleProperty(Rml, StyleAttribute, "font-size");
		Element.FontWeightStyle = ParseStyleProperty(Rml, StyleAttribute, "font-weight");
		Element.TextAlignStyle = ParseStyleProperty(Rml, StyleAttribute, "text-align");
		Element.ColorStyle = ParseStyleProperty(Rml, StyleAttribute, "color");

		const FCssRuleInfo CssRule = FindCssIdRule(OutDocument, Element.Id);
		Element.CssRuleStyleSheetIndex = CssRule.StyleSheetIndex;
		Element.CssRuleInsertPos = CssRule.InsertPos;
		ApplyCssRuleStylesIfMissing(Element, OutDocument, CssRule);

		if (ClassAttribute)
		{
			std::istringstream ClassTokens(ClassAttribute->Value);
			FString ClassToken;
			while (ClassTokens >> ClassToken)
			{
				ApplyCssRuleStylesIfMissing(Element, OutDocument, FindCssClassRule(OutDocument, ClassToken));
			}
		}

		Element.X = ParseStyleFloat(Element.LeftStyle, Element.X);
		Element.Y = ParseStyleFloat(Element.TopStyle, Element.Y);
		Element.Width = ParseStyleFloat(Element.WidthStyle, Element.Width);
		Element.Height = ParseStyleFloat(Element.HeightStyle, Element.Height);
		Element.FontSize = ParseStyleFloat(Element.FontSizeStyle, Element.FontSize);
		if (Element.FontWeightStyle.bExistsInSource)
		{
			Element.FontWeight = TrimCopy(Element.FontWeightStyle.OriginalValue);
			if (Element.FontWeight == "black")
			{
				Element.FontWeight = "900";
			}
		}
		if (Element.TextAlignStyle.bExistsInSource)
		{
			Element.TextAlign = FormatTextAlign(TrimCopy(Element.TextAlignStyle.OriginalValue));
		}
		if (Element.ColorStyle.bExistsInSource)
		{
			ParseCssColor(Element.ColorStyle.OriginalValue, Element.Color);
		}
		Element.bUsePercentLayout =
			IsPercentStyle(Element.LeftStyle) &&
			IsPercentStyle(Element.TopStyle) &&
			IsPercentStyle(Element.WidthStyle) &&
			IsPercentStyle(Element.HeightStyle);

		OutDocument.TextElements.push_back(std::move(Element));
		Cursor = TagEnd;
	}

	OutDocument.bDirty = false;
	return true;
}
