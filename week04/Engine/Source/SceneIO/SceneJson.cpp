#include "SceneJson.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace
{
    class FParser
    {
      public:
        explicit FParser(const FString& InSource)
            : Source(InSource)
        {
        }

        bool Parse(FSceneJsonValue& OutValue, FString* OutErrorMessage)
        {
            SkipWhitespace();

            if (!ParseValue(OutValue))
            {
                if (OutErrorMessage != nullptr && ErrorMessage.empty())
                {
                    *OutErrorMessage = "Unknown JSON parse error.";
                }
                else if (OutErrorMessage != nullptr)
                {
                    *OutErrorMessage = ErrorMessage;
                }

                return false;
            }

            SkipWhitespace();
            if (!IsAtEnd())
            {
                SetError("Unexpected trailing characters.");
                if (OutErrorMessage != nullptr)
                {
                    *OutErrorMessage = ErrorMessage;
                }
                return false;
            }

            return true;
        }

      private:
        bool ParseValue(FSceneJsonValue& OutValue)
        {
            if (IsAtEnd())
            {
                return Fail("Unexpected end of input.");
            }

            switch (Peek())
            {
            case 'n':
                return ParseKeyword("null", OutValue);
            case 't':
                return ParseKeyword("true", OutValue);
            case 'f':
                return ParseKeyword("false", OutValue);
            case '"':
            {
                FString StringValue;
                if (!ParseString(StringValue))
                {
                    return false;
                }
                OutValue = std::move(StringValue);
                return true;
            }
            case '[':
                return ParseArray(OutValue);
            case '{':
                return ParseObject(OutValue);
            default:
                if (Peek() == '-' || std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    double NumberValue = 0.0;
                    if (!ParseNumber(NumberValue))
                    {
                        return false;
                    }
                    OutValue = NumberValue;
                    return true;
                }
                return Fail("Unexpected token.");
            }
        }

        bool ParseKeyword(const char* Keyword, FSceneJsonValue& OutValue)
        {
            const size_t Length = std::strlen(Keyword);
            if (Source.compare(Position, Length, Keyword) != 0)
            {
                return Fail("Invalid literal.");
            }

            Position += Length;

            if (std::strcmp(Keyword, "null") == 0)
            {
                OutValue = FSceneJsonValue(nullptr);
            }
            else if (std::strcmp(Keyword, "true") == 0)
            {
                OutValue = FSceneJsonValue(true);
            }
            else
            {
                OutValue = FSceneJsonValue(false);
            }

            return true;
        }

        bool ParseString(FString& OutValue)
        {
            if (!Consume('"'))
            {
                return Fail("Expected string.");
            }

            FString Result;
            while (!IsAtEnd())
            {
                const char Current = Advance();
                if (Current == '"')
                {
                    OutValue = std::move(Result);
                    return true;
                }

                if (Current == '\\')
                {
                    if (IsAtEnd())
                    {
                        return Fail("Unexpected end of escape sequence.");
                    }

                    const char Escaped = Advance();
                    switch (Escaped)
                    {
                    case '"':
                    case '\\':
                    case '/':
                        Result.push_back(Escaped);
                        break;
                    case 'b':
                        Result.push_back('\b');
                        break;
                    case 'f':
                        Result.push_back('\f');
                        break;
                    case 'n':
                        Result.push_back('\n');
                        break;
                    case 'r':
                        Result.push_back('\r');
                        break;
                    case 't':
                        Result.push_back('\t');
                        break;
                    default:
                        return Fail("Unsupported escape sequence.");
                    }

                    continue;
                }

                Result.push_back(Current);
            }

            return Fail("Unterminated string.");
        }

        bool ParseNumber(double& OutValue)
        {
            const size_t NumberStart = Position;

            if (Peek() == '-')
            {
                Advance();
            }

            if (IsAtEnd())
            {
                return Fail("Invalid number.");
            }

            if (Peek() == '0')
            {
                Advance();
            }
            else
            {
                if (!std::isdigit(static_cast<unsigned char>(Peek())))
                {
                    return Fail("Invalid number.");
                }

                while (!IsAtEnd() &&
                       std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    Advance();
                }
            }

            if (!IsAtEnd() && Peek() == '.')
            {
                Advance();
                if (IsAtEnd() ||
                    std::isdigit(static_cast<unsigned char>(Peek())) == 0)
                {
                    return Fail("Invalid number.");
                }

                while (!IsAtEnd() &&
                       std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    Advance();
                }
            }

            if (!IsAtEnd() && (Peek() == 'e' || Peek() == 'E'))
            {
                Advance();
                if (!IsAtEnd() && (Peek() == '+' || Peek() == '-'))
                {
                    Advance();
                }

                if (IsAtEnd() ||
                    std::isdigit(static_cast<unsigned char>(Peek())) == 0)
                {
                    return Fail("Invalid number.");
                }

                while (!IsAtEnd() &&
                       std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    Advance();
                }
            }

            const FString NumberToken = Source.substr(NumberStart, Position - NumberStart);
            char* EndPtr = nullptr;
            const double ParsedNumber = std::strtod(NumberToken.c_str(), &EndPtr);
            if (EndPtr == NumberToken.c_str() || *EndPtr != '\0')
            {
                return Fail("Invalid number.");
            }

            OutValue = ParsedNumber;
            return true;
        }

        bool ParseArray(FSceneJsonValue& OutValue)
        {
            if (!Consume('['))
            {
                return Fail("Expected array.");
            }

            FSceneJsonValue::Array ArrayValue;
            SkipWhitespace();
            if (Consume(']'))
            {
                OutValue = std::move(ArrayValue);
                return true;
            }

            while (true)
            {
                FSceneJsonValue ElementValue;
                SkipWhitespace();
                if (!ParseValue(ElementValue))
                {
                    return false;
                }

                ArrayValue.push_back(std::move(ElementValue));
                SkipWhitespace();

                if (Consume(']'))
                {
                    OutValue = std::move(ArrayValue);
                    return true;
                }

                if (!Consume(','))
                {
                    return Fail("Expected ',' or ']'.");
                }
                SkipWhitespace();
            }
        }

        bool ParseObject(FSceneJsonValue& OutValue)
        {
            if (!Consume('{'))
            {
                return Fail("Expected object.");
            }

            FSceneJsonValue::Object ObjectValue;
            SkipWhitespace();
            if (Consume('}'))
            {
                OutValue = std::move(ObjectValue);
                return true;
            }

            while (true)
            {
                SkipWhitespace();
                FString Key;
                if (!ParseString(Key))
                {
                    return false;
                }

                SkipWhitespace();
                if (!Consume(':'))
                {
                    return Fail("Expected ':'.");
                }

                FSceneJsonValue FieldValue;
                SkipWhitespace();
                if (!ParseValue(FieldValue))
                {
                    return false;
                }

                ObjectValue[Key] = std::move(FieldValue);
                SkipWhitespace();

                if (Consume('}'))
                {
                    OutValue = std::move(ObjectValue);
                    return true;
                }

                if (!Consume(','))
                {
                    return Fail("Expected ',' or '}'.");
                }
                SkipWhitespace();
            }
        }

        void SkipWhitespace()
        {
            while (!IsAtEnd() &&
                   std::isspace(static_cast<unsigned char>(Source[Position])) != 0)
            {
                ++Position;
            }
        }

        bool Consume(char Expected)
        {
            if (Peek() != Expected)
            {
                return false;
            }

            ++Position;
            return true;
        }

        char Peek() const
        {
            if (IsAtEnd())
            {
                return '\0';
            }

            return Source[Position];
        }

        char Advance()
        {
            if (IsAtEnd())
            {
                return '\0';
            }

            return Source[Position++];
        }

        bool IsAtEnd() const
        {
            return Position >= Source.size();
        }

        bool Fail(const char* Message)
        {
            SetError(Message);
            return false;
        }

        void SetError(const char* Message)
        {
            if (!ErrorMessage.empty())
            {
                return;
            }

            ErrorMessage = FString(Message) + " (offset " + std::to_string(Position) + ")";
        }

      private:
        const FString& Source;
        size_t Position = 0;
        FString ErrorMessage;
    };

    class FWriter
    {
      public:
        explicit FWriter(bool bInPretty)
            : bPretty(bInPretty)
        {
        }

        FString Write(const FSceneJsonValue& Value)
        {
            WriteValue(Value, 0);
            return Buffer;
        }

      private:
        void WriteIndent(int32 Depth)
        {
            if (!bPretty)
            {
                return;
            }

            Buffer.append(static_cast<size_t>(Depth) * 2, ' ');
        }

        void WriteNewLine()
        {
            if (bPretty)
            {
                Buffer.push_back('\n');
            }
        }

        void WriteEscapedString(const FString& Value)
        {
            Buffer.push_back('"');
            for (char Character : Value)
            {
                switch (Character)
                {
                case '"':
                    Buffer += "\\\"";
                    break;
                case '\\':
                    Buffer += "\\\\";
                    break;
                case '\b':
                    Buffer += "\\b";
                    break;
                case '\f':
                    Buffer += "\\f";
                    break;
                case '\n':
                    Buffer += "\\n";
                    break;
                case '\r':
                    Buffer += "\\r";
                    break;
                case '\t':
                    Buffer += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(Character) < 0x20U)
                    {
                        std::ostringstream Stream;
                        Stream << "\\u" << std::hex << std::uppercase
                               << static_cast<int32>(static_cast<unsigned char>(Character));
                        Buffer += Stream.str();
                    }
                    else
                    {
                        Buffer.push_back(Character);
                    }
                    break;
                }
            }
            Buffer.push_back('"');
        }

        void WriteValue(const FSceneJsonValue& Value, int32 Depth)
        {
            if (Value.IsNull())
            {
                Buffer += "null";
                return;
            }

            bool BoolValue = false;
            if (Value.TryGetBool(BoolValue))
            {
                Buffer += BoolValue ? "true" : "false";
                return;
            }

            double NumberValue = 0.0;
            if (Value.TryGetNumber(NumberValue))
            {
                std::ostringstream Stream;
                Stream.precision(15);
                Stream << NumberValue;
                Buffer += Stream.str();
                return;
            }

            FString StringValue;
            if (Value.TryGetString(StringValue))
            {
                WriteEscapedString(StringValue);
                return;
            }

            if (const FSceneJsonValue::Array* ArrayValue = Value.TryGetArray())
            {
                Buffer.push_back('[');
                if (!ArrayValue->empty())
                {
                    WriteNewLine();
                    for (size_t Index = 0; Index < ArrayValue->size(); ++Index)
                    {
                        WriteIndent(Depth + 1);
                        WriteValue((*ArrayValue)[Index], Depth + 1);
                        if (Index + 1 < ArrayValue->size())
                        {
                            Buffer.push_back(',');
                        }
                        WriteNewLine();
                    }
                    WriteIndent(Depth);
                }
                Buffer.push_back(']');
                return;
            }

            const FSceneJsonValue::Object* ObjectValue = Value.TryGetObject();
            Buffer.push_back('{');
            if (ObjectValue != nullptr && !ObjectValue->empty())
            {
                WriteNewLine();
                size_t FieldIndex = 0;
                for (const auto& [Key, FieldValue] : *ObjectValue)
                {
                    WriteIndent(Depth + 1);
                    WriteEscapedString(Key);
                    Buffer += bPretty ? ": " : ":";
                    WriteValue(FieldValue, Depth + 1);
                    if (++FieldIndex < ObjectValue->size())
                    {
                        Buffer.push_back(',');
                    }
                    WriteNewLine();
                }
                WriteIndent(Depth);
            }
            Buffer.push_back('}');
        }

      private:
        bool bPretty = true;
        FString Buffer;
    };
} // namespace

FSceneJsonValue::FSceneJsonValue(std::nullptr_t)
{
}

FSceneJsonValue::FSceneJsonValue(bool InValue)
    : Value(InValue)
{
}

FSceneJsonValue::FSceneJsonValue(double InValue)
    : Value(InValue)
{
}

FSceneJsonValue::FSceneJsonValue(const FString& InValue)
    : Value(InValue)
{
}

FSceneJsonValue::FSceneJsonValue(FString&& InValue)
    : Value(std::move(InValue))
{
}

FSceneJsonValue::FSceneJsonValue(const char* InValue)
    : Value(FString(InValue != nullptr ? InValue : ""))
{
}

FSceneJsonValue::FSceneJsonValue(const Array& InValue)
    : Value(InValue)
{
}

FSceneJsonValue::FSceneJsonValue(Array&& InValue)
    : Value(std::move(InValue))
{
}

FSceneJsonValue::FSceneJsonValue(const Object& InValue)
    : Value(InValue)
{
}

FSceneJsonValue::FSceneJsonValue(Object&& InValue)
    : Value(std::move(InValue))
{
}

FSceneJsonValue::EType FSceneJsonValue::GetType() const
{
    if (std::holds_alternative<std::monostate>(Value))
    {
        return EType::Null;
    }
    if (std::holds_alternative<bool>(Value))
    {
        return EType::Bool;
    }
    if (std::holds_alternative<double>(Value))
    {
        return EType::Number;
    }
    if (std::holds_alternative<FString>(Value))
    {
        return EType::String;
    }
    if (std::holds_alternative<Array>(Value))
    {
        return EType::Array;
    }
    return EType::Object;
}

bool FSceneJsonValue::IsNull() const { return GetType() == EType::Null; }
bool FSceneJsonValue::IsBool() const { return GetType() == EType::Bool; }
bool FSceneJsonValue::IsNumber() const { return GetType() == EType::Number; }
bool FSceneJsonValue::IsString() const { return GetType() == EType::String; }
bool FSceneJsonValue::IsArray() const { return GetType() == EType::Array; }
bool FSceneJsonValue::IsObject() const { return GetType() == EType::Object; }

bool FSceneJsonValue::TryGetBool(bool& OutValue) const
{
    if (!std::holds_alternative<bool>(Value))
    {
        return false;
    }

    OutValue = std::get<bool>(Value);
    return true;
}

bool FSceneJsonValue::TryGetNumber(double& OutValue) const
{
    if (!std::holds_alternative<double>(Value))
    {
        return false;
    }

    OutValue = std::get<double>(Value);
    return true;
}

bool FSceneJsonValue::TryGetString(FString& OutValue) const
{
    if (!std::holds_alternative<FString>(Value))
    {
        return false;
    }

    OutValue = std::get<FString>(Value);
    return true;
}

const FSceneJsonValue::Array* FSceneJsonValue::TryGetArray() const
{
    return std::get_if<Array>(&Value);
}

FSceneJsonValue::Array* FSceneJsonValue::TryGetArray()
{
    return std::get_if<Array>(&Value);
}

const FSceneJsonValue::Object* FSceneJsonValue::TryGetObject() const
{
    return std::get_if<Object>(&Value);
}

FSceneJsonValue::Object* FSceneJsonValue::TryGetObject()
{
    return std::get_if<Object>(&Value);
}

const FSceneJsonValue* FSceneJsonValue::FindField(const FString& Key) const
{
    const Object* ObjectValue = TryGetObject();
    if (ObjectValue == nullptr)
    {
        return nullptr;
    }

    const auto Iterator = ObjectValue->find(Key);
    if (Iterator == ObjectValue->end())
    {
        return nullptr;
    }

    return &Iterator->second;
}

FSceneJsonValue* FSceneJsonValue::FindField(const FString& Key)
{
    Object* ObjectValue = TryGetObject();
    if (ObjectValue == nullptr)
    {
        return nullptr;
    }

    const auto Iterator = ObjectValue->find(Key);
    if (Iterator == ObjectValue->end())
    {
        return nullptr;
    }

    return &Iterator->second;
}

bool FSceneJsonParser::Parse(const FString& Source, FSceneJsonValue& OutValue,
                             FString* OutErrorMessage)
{
    FParser Parser(Source);
    return Parser.Parse(OutValue, OutErrorMessage);
}

FString FSceneJsonWriter::Write(const FSceneJsonValue& Value, bool bPretty)
{
    FWriter Writer(bPretty);
    return Writer.Write(Value);
}
