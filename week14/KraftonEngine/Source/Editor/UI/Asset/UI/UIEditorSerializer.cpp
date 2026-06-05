#include "Editor/UI/Asset/UI/UIEditorSerializer.h"

#include "Platform/Paths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

namespace
{
	void SetUIEditorError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
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

	FString ExtractAttribute(const FString& Tag, const char* AttributeName)
	{
		const FString Pattern = FString(AttributeName) + R"rml(\s*=\s*"([^"]*)")rml";
		const std::regex Regex(Pattern, std::regex::icase);
		std::smatch Match;
		if (std::regex_search(Tag, Match, Regex) && Match.size() > 1)
		{
			return DecodeXml(Match[1].str());
		}
		return {};
	}

	float ExtractStyleFloat(const FString& Style, const char* PropertyName, float DefaultValue)
	{
		const FString Pattern = FString(PropertyName) + R"(\s*:\s*([-+]?[0-9]*\.?[0-9]+))";
		const std::regex Regex(Pattern, std::regex::icase);
		std::smatch Match;
		if (std::regex_search(Style, Match, Regex) && Match.size() > 1)
		{
			return std::stof(Match[1].str());
		}
		return DefaultValue;
	}

	FString ExtractStyleString(const FString& Style, const char* PropertyName, const FString& DefaultValue)
	{
		const FString Pattern = FString(PropertyName) + R"rml(\s*:\s*([^;"]+))rml";
		const std::regex Regex(Pattern, std::regex::icase);
		std::smatch Match;
		if (std::regex_search(Style, Match, Regex) && Match.size() > 1)
		{
			return TrimCopy(Match[1].str());
		}
		return DefaultValue;
	}
}

bool FUIEditorSerializer::Load(const std::filesystem::path& Path, FUIEditorDocument& OutDocument, FString* OutError)
{
	FString Rml;
	if (std::filesystem::exists(Path))
	{
		if (!ReadTextFile(Path, Rml, OutError))
		{
			return false;
		}
	}
	else
	{
		Rml = BuildRml(OutDocument);
	}

	OutDocument = FUIEditorDocument {};
	OutDocument.SourcePath = Path;
	OutDocument.OriginalRml = Rml;
	OutDocument.bDirty = false;

	return ParseEditableTextElements(Rml, OutDocument, OutError);
}

bool FUIEditorSerializer::Save(const FUIEditorDocument& Document, FString* OutError)
{
	if (Document.SourcePath.empty())
	{
		SetUIEditorError(OutError, "RML source path is empty.");
		return false;
	}

	return WriteTextFile(Document.SourcePath, BuildRml(Document), OutError);
}

bool FUIEditorSerializer::ParseEditableTextElements(const FString& Rml, FUIEditorDocument& OutDocument, FString* OutError)
{
	(void)OutError;

	const std::regex DivRegex(R"(<div\b(?=[^>]*\bdata-ui-editor\s*=\s*"text")[^>]*>[\s\S]*?</div>)", std::regex::icase);
	auto Begin = std::sregex_iterator(Rml.begin(), Rml.end(), DivRegex);
	auto End = std::sregex_iterator();

	for (auto It = Begin; It != End; ++It)
	{
		const FString Block = It->str();
		const size_t TagEnd = Block.find('>');
		if (TagEnd == FString::npos)
		{
			continue;
		}

		const FString OpenTag = Block.substr(0, TagEnd + 1);
		const size_t CloseTag = Block.rfind("</div>");
		const FString InnerText = CloseTag != FString::npos && CloseTag > TagEnd
			? Block.substr(TagEnd + 1, CloseTag - TagEnd - 1)
			: FString {};

		FUIEditorTextElement Element;
		Element.Id = ExtractAttribute(OpenTag, "id");
		Element.Text = DecodeXml(TrimCopy(InnerText));

		const FString Style = ExtractAttribute(OpenTag, "style");
		Element.X = ExtractStyleFloat(Style, "left", Element.X);
		Element.Y = ExtractStyleFloat(Style, "top", Element.Y);
		Element.Width = ExtractStyleFloat(Style, "width", Element.Width);
		Element.Height = ExtractStyleFloat(Style, "height", Element.Height);
		Element.FontSize = ExtractStyleFloat(Style, "font-size", Element.FontSize);
		Element.FontWeight = ExtractStyleString(Style, "font-weight", Element.FontWeight);

		OutDocument.TextElements.push_back(std::move(Element));
	}

	return true;
}

FString FUIEditorSerializer::BuildRml(const FUIEditorDocument& Document)
{
	std::ostringstream Out;
	Out << "<rml>\n";
	Out << "<head>\n";
	Out << "    <title>Generated UI</title>\n";
	Out << "    <style>\n";
	Out << "        body\n";
	Out << "        {\n";
	Out << "            width: 100%;\n";
	Out << "            height: 100%;\n";
	Out << "            margin: 0px;\n";
	Out << "        }\n\n";
	Out << "        .ui-text\n";
	Out << "        {\n";
	Out << "            position: absolute;\n";
	Out << "            color: white;\n";
	Out << "            font-family: Pretendard;\n";
	Out << "        }\n";
	Out << "    </style>\n";
	Out << "</head>\n\n";
	Out << "<body>\n";

	for (const FUIEditorTextElement& Element : Document.TextElements)
	{
		const float Width = (std::max)(1.0f, Element.Width);
		const float Height = (std::max)(1.0f, Element.Height);
		const float FontSize = (std::max)(1.0f, Element.FontSize);

		char StyleBuffer[256];
		std::snprintf(
			StyleBuffer,
			sizeof(StyleBuffer),
			"left: %.2fpx; top: %.2fpx; width: %.2fpx; height: %.2fpx; font-size: %.2fpx; font-weight: %s;",
			Element.X,
			Element.Y,
			Width,
			Height,
			FontSize,
			Element.FontWeight == "bold" ? "bold" : "normal"
		);

		Out << "    <div id=\"" << EncodeXml(Element.Id) << "\"\n";
		Out << "         class=\"ui-text\"\n";
		Out << "         data-ui-editor=\"text\"\n";
		Out << "         style=\"" << StyleBuffer << "\">\n";
		Out << "        " << EncodeXml(Element.Text) << "\n";
		Out << "    </div>\n\n";
	}

	Out << "</body>\n";
	Out << "</rml>\n";
	return Out.str();
}
